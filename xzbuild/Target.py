import abc
import glob
import inspect
import os

from . import COLOR, writeItems, writeItem
from . import PowerList as PList
from .Environment import findAppInPath


class BuildTarget(metaclass=abc.ABCMeta):
    @staticmethod
    @abc.abstractmethod
    def prefix() -> str:
        pass
    @staticmethod
    def initEnv(env: dict):
        '''initialize environment'''
        pass
    @staticmethod
    def modifyProject(proj, env:dict):
        '''modify parent project'''
        pass

    def porcPathDefine(self, path:str):
        return os.path.normpath(path.replace("$(SolutionDir)", self.solDir).replace("$(BuildDir)", self.buildDir))
    
    def write(self, file):
        file.write(f"\n\n# For target [{self.prefix()}]\n")
        writeItems(file, self.prefix()+"_srcs", self.sources)
        writeItems(file, self.prefix()+"_flags", self.flags)

    def printSources(self):
        print(f'{COLOR.magenta}[{self.prefix()}] Sources:\n{COLOR.White(" ".join(self.sources))}')

    def __init__(self, targets:dict, proj, env:dict):
        self.sources = []
        self.flags = []
        self.solDir = env["rootDir"]
        self.buildDir = os.path.join(self.solDir, proj.buildPath)
        self.solveTarget(targets, proj, env)
        if hasattr(self, "incpath"):
            self.incpath = [self.porcPathDefine(path) for path in self.incpath]

    def solveSource(self, targets:dict, env:dict):
        def adddir(ret:tuple, ele:dict, env:dict):
            if "dir" in ele:
                dir = self.porcPathDefine(ele["dir"])
                return tuple(list(os.path.join(dir, i) for i in x) for x in ret)
            return ret
        target = targets[self.prefix()]
        a,d = PList.solveElementList(target, "sources", env, adddir)
        adds = set(f for i in a for f in glob.glob(i))
        dels = set(f for i in d for f in glob.glob(i))
        forceadd = adds & set(a)
        forcedel = dels & set(d)
        self.sources = list(((adds - dels) | forceadd) - forcedel)
        self.sources.sort()

    def solveTarget(self, targets:dict, proj, env:dict):
        '''solve sources and flags'''
        self.solveSource(targets, env)
        target = targets[self.prefix()]
        a,d = PList.solveElementList(target, "flags", env)
        self.flags = PList.combineElements(self.flags, a, d)

    def __repr__(self):
        return str(vars(self))


class CXXTarget(BuildTarget, metaclass=abc.ABCMeta):
    @staticmethod
    @abc.abstractmethod
    def langVersion() -> str:
        pass

    def __init__(self, targets:dict, proj, env:dict):
        self.defines = []
        self.incpath = []
        self.pch = ""
        self.dbgSymLevel = "3"
        self.optimize = ""
        self.version = ""
        self.visibility = "hidden"
        super().__init__(targets, proj, env)

    def solveTarget(self, targets:dict, proj, env:dict):
        self.flags += ["-Wall", "-pedantic", "-pthread", "-Wno-unknown-pragmas", "-Wno-ignored-attributes", "-Wno-unused-local-typedefs", "-fno-common"]
        self.flags += [env["archparam"]]
        if env["gprof"] == True:
            self.flags += ["-pg"]
        if env["arch"] == "x86":
            self.flags += ["-m64" if env["bits"] == 64 else "-m32"]
        self.optimize = "-O2" if env["target"] == "Release" else "-O0"
        if env["compiler"] == "clang":
            self.flags += ["-Wno-newline-eof"]
        if env["target"] == "Release":
            self.defines += ["NDEBUG"]
            if env["paras"].get("lto", "on") == "on":
                self.flags += ["-flto"]
        if "iOS" in env:
            verstr = f"{int(env['iOSVer'] / 10000)}.{int((env['iOSVer'] / 100) % 100)}"
            self.flags += ["-miphoneos-version-min=" + verstr]
        if "dsym" in env:
            self.flags += ["-g" + env["dsym"]]
        cxx = targets.get("cxx")
        if cxx is not None:
            self.visibility = cxx.get("visibility", self.visibility)
            a,_ = PList.solveElementList(cxx, "dbgSymLevel", env)
            self.dbgSymLevel = a[0] if len(a)>0 else self.dbgSymLevel
            a,_ = PList.solveElementList(cxx, "optimize", env)
            self.optimize = a[0] if len(a)>0 else self.optimize
            a,d = PList.solveElementList(cxx, "flags", env)
            self.flags = PList.combineElements(self.flags, a, d)
            a,d = PList.solveElementList(cxx, "defines", env)
            self.defines = PList.combineElements(self.defines, a, d)
            a,d = PList.solveElementList(cxx, "incpath", env)
            self.incpath = PList.combineElements(self.incpath, a, d)
        super().solveTarget(targets, proj, env)
        target = targets[self.prefix()]
        self.pch = target.get("pch", "")
        self.version = target.get("version", self.langVersion())
        self.visibility = target.get("visibility", self.visibility)
        a,_ = PList.solveElementList(target, "dbgSymLevel", env)
        self.dbgSymLevel = a[0] if len(a)>0 else self.dbgSymLevel
        a,_ = PList.solveElementList(target, "optimize", env)
        self.optimize = a[0] if len(a)>0 else self.optimize
        a,d = PList.solveElementList(target, "defines", env)
        self.defines = PList.combineElements(self.defines, a, d)
        a,d = PList.solveElementList(target, "incpath", env)
        self.incpath = PList.combineElements(self.incpath, a, d)
        self.dbgSymLevel = env.get("dslv", self.dbgSymLevel)
        self.flags += [f"-fvisibility={self.visibility}"]

    def write(self, file):
        super().write(file)
        writeItems(file, self.prefix()+"_defs", self.defines)
        writeItems(file, self.prefix()+"_incpaths", self.incpath)
        writeItems(file, self.prefix()+"_flags", [self.version, "-g" + self.dbgSymLevel, self.optimize], state="+")
        writeItem (file, self.prefix()+"_pch", self.pch)


class CPPTarget(CXXTarget):
    @staticmethod
    def prefix() -> str:
        return "cpp"
    @staticmethod
    def langVersion() -> str:
        return "-std=c++17"
    def solveTarget(self, targets, proj, env:dict):
        super().solveTarget(targets, proj, env)
        # it is agains c++ rules because class members also got influnced
        # https://stackoverflow.com/questions/48621251/why-fvisibility-inlines-hidden-is-not-the-default
        # if self.visibility == "hidden":
        #     self.flags += ["-fvisibility-inlines-hidden"]


class CTarget(CXXTarget):
    @staticmethod
    def prefix() -> str:
        return "c"
    @staticmethod
    def langVersion() -> str:
        return "-std=c11"


class ASMTarget(CXXTarget):
    @staticmethod
    def prefix() -> str:
        return "asm"
    @staticmethod
    def langVersion() -> str:
        return ""


class NASMTarget(BuildTarget):
    @staticmethod
    def prefix() -> str:
        return "nasm"
    def __init__(self, targets:dict, proj, env:dict):
        self.incpath = []
        self.defines = []
        super().__init__(targets, proj, env)

    def solveTarget(self, targets:dict, proj, env:dict):
        self.flags = ["-g"]
        if env["platform"] == "x64":
            self.flags += ["-f macho64" if env["osname"] == "Darwin" else "-f elf64"]
        elif env["platform"] == "x86":
            self.flags += ["-f macho32" if env["osname"] == "Darwin" else "-f elf32"]
        super().solveTarget(targets, proj, env)
        target = targets[self.prefix()]
        a,d = PList.solveElementList(target, "defines", env)
        self.defines = PList.combineElements(self.defines, a, d)
        a,d = PList.solveElementList(target, "incpath", env)
        self.incpath = list(set(a) - set(d))
        self.flags += [f"-D{x}" for x in self.defines]

    def write(self, file):
        super().write(file)
        writeItems(file, self.prefix()+"_incpaths", self.incpath)


class RCTarget(BuildTarget):
    @staticmethod
    def prefix() -> str:
        return "rc"


class ISPCTarget(BuildTarget):
    compiler = None
    @staticmethod
    def prefix() -> str:
        return "ispc"
    @staticmethod
    def initEnv(env: dict):
        if "ISPCCOMPILER" not in os.environ:
            paths = findAppInPath("ispc")
            if len(paths) > 0:
                ISPCTarget.compiler = os.path.join(paths[0], "ispc")
            else:
                print(COLOR.Yellow("Seems ISPC is not found"))


    def __init__(self, targets:dict, proj, env:dict):
        if env["arch"] == "x86":
            self.targets = ["sse4", "avx2"]
        elif env["arch"] == "arm":
            self.targets = ["neon"]
        super().__init__(targets, proj, env)

    def solveTarget(self, targets:dict, proj, env:dict):
        self.flags = ["-g", "-O2", "--opt=fast-math", "--pic"]
        if env["platform"] == "x64":
            self.flags += ["--arch=x86-64"]
        elif env["platform"] == "x86":
            self.flags += ["--arch=x86"]
        elif env["platform"] == "ARM":
            self.flags += ["--arch=arm"]
        super().solveTarget(targets, proj, env)

    def write(self, file):
        super().write(file)
        writeItems(file, self.prefix()+"_targets", self.targets)
        writeItem(file, self.prefix()+"_flags", "--target="+(",".join(self.targets)), state="+")
        if ISPCTarget.compiler:
            writeItem(file, "ISPCCOMPILER", ISPCTarget.compiler, state="?")
            pass


class CUDATarget(BuildTarget):
    cudaHome = None
    @staticmethod
    def prefix() -> str:
        return "cuda"
    @staticmethod
    def initEnv(env: dict):
        CUDATarget.cudaHome = os.environ.get("CUDA_PATH")
        if CUDATarget.cudaHome == None:
            paths = findAppInPath("nvcc")
            if len(paths) > 0:
                CUDATarget.cudaHome = os.path.abspath(os.path.join(paths[0], os.pardir))
        if env["platform"] == "x86":
            print(COLOR.Yellow("Latest CUDA does not support 32bit any more"))

    @staticmethod
    def modifyProject(proj, env:dict):
        proj.libDynamic += ["cuda", "cudart"]
        if CUDATarget.cudaHome:
            proj.libDirs += [os.path.join(CUDATarget.cudaHome, "lib64")]

    def __init__(self, targets:dict, proj, env:dict):
        self.defines = []
        self.incpath = []
        self.hostDebug = "-g"
        self.deviceDebug = ""
        self.optimize = ""
        self.version = ""
        self.arch = []
        super().__init__(targets, proj, env)
        if CUDATarget.cudaHome:
            self.incpath += [os.path.join(CUDATarget.cudaHome, "include")]
        else:
            print(COLOR.Yellow("CUDA Home directory not found"))

    def solveTarget(self, targets:dict, proj, env:dict):
        self.flags += ["-pg", "-lineinfo", "-use_fast_math", "-res-usage", "--source-in-ptx"]
        self.flags += ["-m64" if env["platform"] == "x64" else "-m32"]
        self.optimize = "-O2" if env["target"] == "Release" else "-O0"
        if env["target"] == "Release":
            self.defines += ["NDEBUG"]
        cuda = targets.get("cuda")
        if cuda is not None:
            a,_ = PList.solveElementList(cuda, "hostDebug", env)
            self.hostDebug = a[0] if len(a)>0 else self.hostDebug
            a,_ = PList.solveElementList(cuda, "deviceDebug", env)
            self.deviceDebug = a[0] if len(a)>0 else self.deviceDebug
            a,_ = PList.solveElementList(cuda, "optimize", env)
            self.optimize = a[0] if len(a)>0 else self.optimize
            a,d = PList.solveElementList(cuda, "flags", env)
            self.flags = PList.combineElements(self.flags, a, d)
            a,d = PList.solveElementList(cuda, "defines", env)
            self.defines = PList.combineElements(self.defines, a, d)
            a,d = PList.solveElementList(cuda, "incpath", env)
            self.incpath = PList.combineElements(self.incpath, a, d)
            a,d = PList.solveElementList(cuda, "arch", env)
            self.arch = PList.combineElements(self.arch, a, d)
            self.version = cuda.get("version", "-std=c++14")
        super().solveTarget(targets, proj, env)

    def write(self, file):
        super().write(file)
        writeItems(file, self.prefix()+"_defs", self.defines)
        writeItems(file, self.prefix()+"_incpaths", self.incpath)
        writeItems(file, self.prefix()+"_flags", [self.version, self.hostDebug, self.deviceDebug, self.optimize], state="+")
        if len(self.arch) > 0:
            writeItem(file, self.prefix()+"_flags", "-arch="+",".join(self.arch), state="+")



def _getSubclasses(clz):
    for c in clz.__subclasses__():
        if inspect.isabstract(c):
            for subc in _getSubclasses(c):
                yield subc
        else:
            yield c

_AllTargets = list(_getSubclasses(BuildTarget))