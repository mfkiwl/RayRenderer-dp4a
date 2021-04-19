import json
import os
import errno
import platform
import subprocess
import sys
import time


_intrinMap = \
{
    "__SSE__":    "sse",
    "__SSE2__":   "sse2",
    "__SSE3__":   "sse3",
    "__SSSE3__":  "ssse3",
    "__SSE4_1__": "sse41",
    "__SSE4_2__": "sse42",
    "__AVX__":    "avx",
    "__FMA__":    "fma",
    "__AVX2__":   "avx2",
    "__AES__":    "aes",
    "__SHA__":    "sha",
    "__PCLMUL__": "pclmul",
}

def _checkExists(prog:str) -> bool:
    FNULL = open(os.devnull, 'w')
    try:
        subprocess.call([prog], stdout=FNULL, stderr=FNULL)
    except OSError as e:
        if e.errno == errno.ENOENT:
            return False
    return True

def splitPaths(path:str):
    if path == None:
        return []
    return [p for p in path.split(os.pathsep) if p]

def findFileInPath(fname:str):
    paths = splitPaths(os.environ["PATH"])
    return [p for p in paths if os.path.isfile(os.path.join(p, fname))]
    #return next((p for p in paths if os.path.isfile(os.path.join(p, fname))), None)

def findAppInPath(appname:str):
    osname = platform.system()
    return findFileInPath(appname+".exe" if osname == "Windows" else appname)

def collectEnv(paras:dict) -> dict:
    solDir = os.getcwd()
    xzbuildPath = os.path.relpath(os.path.abspath(os.path.dirname(__file__)), solDir)
    env = {"rootDir": solDir, "xzbuildPath": xzbuildPath, "target": "Debug", "paras": paras}
    env["verbose"] = "verbose" in paras
    is64Bits = sys.maxsize > 2**32
    env["platform"] = "x64" if is64Bits else "x86"
    env["incDirs"] = []
    env["libDirs"] = []
    env["defines"] = []
    env["incDirs"] += [x+"/include" for x in [os.environ.get("CPP_DEPENDENCY_PATH")] if x is not None]
    
    cppcompiler = os.environ.get("CPPCOMPILER", "c++")
    ccompiler = os.environ.get("CCOMPILER", "cc")
    env["cppcompiler"] = cppcompiler
    env["ccompiler"] = ccompiler
    env["osname"] = platform.system()
    env["machine"] = platform.machine()
    if env["machine"] in ["i386", "AMD64", "x86", "x86_64"]:
        env["arch"] = "x86"
    elif env["machine"] in ["arm", "aarch64_be", "aarch64", "armv8b", "armv8l"]:
        env["arch"] = "arm"
    else:
        env["arch"] = ""
    
    targetarch = paras.get("targetarch", "native")
    defs = {}
    if not env["osname"] == "Windows":
        qarg = "march" if env["arch"] == "x86" else "mcpu"
        env["archparam"] = f"-{qarg}={targetarch}"
        rawdefs = subprocess.check_output(f"{cppcompiler} {env['archparam']} -xc++ -dM -E - < /dev/null", shell=True)
        defs = dict([d.split(" ", 2)[1:3] for d in rawdefs.decode().splitlines()])
        env["libDirs"] += splitPaths(os.environ.get("LD_LIBRARY_PATH"))
        env["compiler"] = "clang" if "__clang__" in defs else "gcc"
        arlinker = "gcc-ar" if env["compiler"] == "gcc" else "ar"
        if not _checkExists(arlinker): arlinker = "ar"
        env["arlinker"] = os.environ.get("STATICLINKER", arlinker)
        if env["compiler"] == "clang":
            env["clangVer"] = int(defs["__clang_major__"]) * 10000 + int(defs["__clang_minor__"]) * 100 + int(defs["__clang_patchlevel__"])
        else:
            env["gccVer"] = int(defs["__GNUC__"]) * 10000 + int(defs["__GNUC_MINOR__"]) * 100 + int(defs["__GNUC_PATCHLEVEL__"])
    else:
        env["compiler"] = "msvc"
    env["intrin"] = set(i[1] for i in _intrinMap.items() if i[0] in defs)
    if "__ANDROID__" in defs:
        env["android"] = True
    
    env["cpuCount"] = os.cpu_count()
    threads = paras.get("threads", "x1")
    if threads.startswith('x'):
        env["threads"] = int(env["cpuCount"] * float(threads[1:]))
    else:
        env["threads"] = int(threads)

    termuxVer = os.environ.get("TERMUX_VERSION")
    if termuxVer is not None:
        ver = termuxVer.split(".")
        env["termuxVer"] = int(ver[0]) * 10000 + int(ver[1])
        pkgs = subprocess.check_output("pkg list-installed", shell=True)
        libcxx = [x for x in pkgs.decode().splitlines() if x.startswith("libc++")][0]
        libcxxVer = libcxx.split(",")[1].split(" ")[1].split("-")[0]
        env["ndkVer"] = int(libcxxVer[:-1]) * 100 + (ord(libcxxVer[-1]) - ord("a"))

    return env

def writeEnv(env:dict):
    os.makedirs("./" + env["objpath"], exist_ok=True)
    with open(os.path.join(env["objpath"], "xzbuild.sol.mk"), 'w') as file:
        file.write("# xzbuild per solution file\n")
        file.write("# written at [{}]\n".format(time.asctime(time.localtime(time.time()))))
        for k,v in env.items():
            if isinstance(v, (str, int)):
                file.write("xz_{}\t = {}\n".format(k, v))
        file.write("xz_incDir\t = {}\n".format(" ".join(env["incDirs"])))
        file.write("xz_libDir\t = {}\n".format(" ".join(env["libDirs"])))
        file.write("xz_define\t = {}\n".format(" ".join(env["defines"])))
        file.write("STATICLINKER\t = {}\n".format(env["arlinker"]))