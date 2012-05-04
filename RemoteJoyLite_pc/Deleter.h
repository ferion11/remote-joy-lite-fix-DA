#ifndef DELETER_H
#define DELETER_H

#include <windows.h>

struct DeviceNotificationDeleter {
	typedef HDEVNOTIFY pointer;
	void operator ()(HDEVNOTIFY handle);
};

struct HandleDeleter {
	typedef HANDLE pointer;
	void operator ()(HANDLE handle);
};

#endif
