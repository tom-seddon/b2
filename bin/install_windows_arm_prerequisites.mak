SHELL:=$(windir)\system32\cmd.exe

# Work around for this one, I think: https://github.com/actions/runner-images/issues/9873

# See, e.g., https://github.com/mhammond/pywin32/blob/main/.github/workflows/install-vs-components.py

INSTALLER_PATH:=C:/Program Files (x86)/Microsoft Visual Studio/Installer
VSWHERE:=$(INSTALLER_PATH)/vswhere.exe
VSINSTALLER:=$(INSTALLER_PATH)/vs_installer.exe

VS_INSTALL_PATH:=$(shell "$(VSWHERE)" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath)

.PHONY:default
default:
	"$(VSWHERE)" -latest -products * -requires Microsoft.Component.MSBuild -format json
	$(if $(VS_INSTALL_PATH),,$(error Failed to find Visual Studio install path))
	"$(VSINSTALLER)" modify --installPath "$(VS_INSTALL_PATH)" --add Microsoft.VisualStudio.Component.VC.14.29.16.11.ATL.ARM64 --norestart --wait --passive
