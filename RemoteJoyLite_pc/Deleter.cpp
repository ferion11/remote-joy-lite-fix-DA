#include "Deleter.h"

void DeviceNotificationDeleter::operator ()(HDEVNOTIFY handle) {
	if (handle) {
		::UnregisterDeviceNotification(handle);
		handle = NULL;
	}
}

void HandleDeleter::operator ()(HANDLE handle) {
	if (handle) {
		::CloseHandle(handle);
		handle = NULL;
	}
}
