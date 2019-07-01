#include "stdafx.h"
#include "event handle.h"

using namespace std;
using namespace Renderer;

Impl::EventHandle::EventHandle() : handle(CreateEvent(NULL, FALSE, FALSE, NULL))
{
	if (!handle)
		throw HRESULT_FROM_WIN32(GetLastError());
}

Impl::EventHandle::~EventHandle()
{
	if (!CloseHandle(handle))
		cerr << "Fail to close event handle (hr=" << HRESULT_FROM_WIN32(GetLastError()) << ")." << endl;
}