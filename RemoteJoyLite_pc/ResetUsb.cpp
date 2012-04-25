#include <string>
#include <windows.h>
#include <SetupAPI.h>

using namespace std;

static void _UsbResetDevice(HDEVINFO& devInfo) {
	devInfo = SetupDiGetClassDevs(NULL, 0, 0, DIGCF_PRESENT | DIGCF_ALLCLASSES);
	if (devInfo == INVALID_HANDLE_VALUE) {
		return;
	}

	SP_DEVINFO_DATA devInfoData = {0};
	devInfoData.cbSize = sizeof(devInfoData);
	for (int devIndex = 0; SetupDiEnumDeviceInfo(devInfo, devIndex, &devInfoData); devIndex++) {
		DWORD dataType;
        TCHAR buffer[1024];

		if (!SetupDiGetDeviceRegistryProperty(devInfo, &devInfoData, SPDRP_HARDWAREID, &dataType, (PBYTE)buffer, sizeof(buffer) / sizeof(buffer[0]), NULL)) {
			continue;
        }

		static char hardwareId[1024];
		WideCharToMultiByte(CP_ACP, 0, buffer, -1, hardwareId, sizeof(hardwareId), NULL, NULL);


		if (std::string(hardwareId).find("USB\\VID_054C&PID_01C9") == std::string::npos) {
			continue;
		}

		SP_PROPCHANGE_PARAMS propchangeParams = {0};
		propchangeParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
		propchangeParams.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        propchangeParams.StateChange = DICS_PROPCHANGE;
        propchangeParams.Scope = DICS_FLAG_CONFIGSPECIFIC;
        propchangeParams.HwProfile = 0;

		if (!SetupDiSetClassInstallParams(devInfo, &devInfoData, &propchangeParams.ClassInstallHeader, sizeof(propchangeParams))) {
			return;
		}
		
		if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, devInfo, &devInfoData)) {
			return;
        }
	}
}

static void UsbResetDevice() {
	HDEVINFO devs = NULL;
	_UsbResetDevice(devs);

	if (devs != INVALID_HANDLE_VALUE) {
		SetupDiDestroyDeviceInfoList(devs);
		devs = INVALID_HANDLE_VALUE;
	}
}

int main() {
	UsbResetDevice();
}
