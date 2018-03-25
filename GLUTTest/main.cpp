#include "RenderCore/RenderCore.h"
#include <cstdint>
#include <cstdio>
#include <memory>
#include "FreeGLUTView/FreeGLUTView.h"


using namespace glutview;
using namespace b3d;
using std::string;
using std::wstring;
using std::u16string;
using std::vector;
using namespace common;

//OGLU_OPTIMUS_ENABLE_NV

std::unique_ptr<rayr::BasicTest> tester;
uint16_t curObj = 0;
FreeGLUTView window, wd2;
bool isAnimate = false;

void onResize(FreeGLUTView wd, int w, int h)
{
	tester->Resize(w & 0xffc0, h & 0xffc0);
}

void onKeyboard(FreeGLUTView wd, KeyEvent keyevent)
{
	switch (keyevent.SpecialKey())
	{
	case Key::Up:
		tester->Moveobj(curObj, 0, 0.1f, 0); break;
	case Key::Down:
		tester->Moveobj(curObj, 0, -0.1f, 0); break;
	case Key::Left:
		tester->Moveobj(curObj, -0.1f, 0, 0); break;
	case Key::Right:
		tester->Moveobj(curObj, 0.1f, 0, 0); break;
	case Key::PageUp:
		tester->Moveobj(curObj, 0, 0, -0.1f); break;
	case Key::PageDown:
		tester->Moveobj(curObj, 0, 0, 0.1f); break;
	case Key::UNDEFINE:
		switch (keyevent.key)
		{
		case (uint8_t)Key::ESC:
			window.release();
			return;
		case 'a'://pan to left
			tester->cam.yaw(3); break;
		case 'd'://pan to right
			tester->cam.yaw(-3); break;
		case 'w'://pan to up
			tester->cam.pitch(3); break;
		case 's'://pan to down
			tester->cam.pitch(-3); break;
		case 'q'://pan to left
			tester->cam.roll(-3); break;
		case 'e'://pan to left
			tester->cam.roll(3); break;
		case 'A':
			tester->Rotateobj(curObj, 0, 3, 0); break;
		case 'D':
			tester->Rotateobj(curObj, 0, -3, 0); break;
		case 'W':
			tester->Rotateobj(curObj, 3, 0, 0); break;
		case 'S':
			tester->Rotateobj(curObj, -3, 0, 0); break;
		case 'Q':
			tester->Rotateobj(curObj, 0, 0, 3); break;
		case 'E':
			tester->Rotateobj(curObj, 0, 0, -3); break;
		case 'x':
			wd2.reset(800, 600);
			break;
		case 'X':
			wd2.release(); break;
		case 13:
			if (keyevent.hasShift())
				isAnimate = !isAnimate;
			else
				tester->mode = !tester->mode;
			break;
		case '+':
			curObj++;
			if (curObj >= (uint16_t)(tester->Objects().size()))
				curObj = 0;
			tester->showObject(curObj);
			break;
		case '-':
			if (curObj == 0)
				curObj = (uint16_t)(tester->Objects().size());
			curObj--;
			tester->showObject(curObj);
			break;
		}
		printf("U %.4f,%.4f,%.4f\nV %.4f,%.4f,%.4f\nN %.4f,%.4f,%.4f\n",
			tester->cam.u.x, tester->cam.u.y, tester->cam.u.z, tester->cam.v.x, tester->cam.v.y, tester->cam.v.z, tester->cam.n.x, tester->cam.n.y, tester->cam.n.z);
		break;
	}
	wd->refresh();
}

void onMouseEvent(FreeGLUTView wd, MouseEvent msevent)
{
	static const char *tpname[] = { "DOWN", "UP", "MOVE", "OVER", "WHEEL" };
	//printf("%5s mouse %d on window%d\n", tpname[(uint8_t)msevent.type], (uint8_t)msevent.btn, id);
	switch (msevent.type)
	{
	case MouseEventType::Moving:
		tester->cam.move((msevent.dx * 10.f / tester->cam.width), (msevent.dy * 10.f / tester->cam.height), 0);
		break;
	case MouseEventType::Wheel:
		tester->cam.move(0, 0, (float)msevent.dx);
		printf("camera at %5f,%5f,%5f\n", tester->cam.position.x, tester->cam.position.y, tester->cam.position.z);
		break;
	default:
		return;
	}
	wd->refresh();
}

void autoRotate()
{
	tester->Rotateobj(curObj, 0, 3, 0);
}

bool onTimer(FreeGLUTView wd, uint32_t elapseMS)
{
	if (isAnimate)
	{
		autoRotate();
		wd->refresh();
	}
	return true;
}

void onDropFile(FreeGLUTView wd, wstring fname)
{
	static bool isFirst = true;
	if (true)
	{
		tester->AddModelAsync(*(u16string*)&fname, [&, wd](auto cb)
		{
			wd->invoke([&, cb]
			{
				if (cb())
				{
					curObj = (uint16_t)(tester->Objects().size() - 1);
					tester->Rotateobj(curObj, -90, 0, 0);
					tester->Moveobj(curObj, -1, 0, 0);
					return true;
				}
				return false;
			});
		});
	}
	else if (tester->AddModel(*(u16string*)&fname))
	{
        curObj = (uint16_t)(tester->Objects().size() - 1);
		tester->Rotateobj(curObj, -90, 0, 0);
		tester->Moveobj(curObj, 1, 0, 0);
		wd->refresh();
	}
	isFirst = false;
}

int wmain(int argc, wchar_t *argv[], wchar_t *envp[])
{
	printf("miniBLAS intrin:%s\n", miniBLAS::miniBLAS_intrin());
	FreeGLUTViewInit();
	
	window.reset();
	tester.reset(new rayr::BasicTest());
	window->funDisp = [&](FreeGLUTView wd) { tester->Draw(); };
	window->funReshape = onResize;
	window->setTitle("2D");
	window->funKeyEvent = onKeyboard;
	window->funMouseEvent = onMouseEvent;
	window->setTimerCallback(onTimer, 20);
	window->funDropFile = onDropFile;


	FreeGLUTViewRun();
}