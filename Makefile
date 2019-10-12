TARGET = ../../bin/osx_ios_device_trigger

all: $(TARGET)

$(TARGET): osx_ios_device_trigger/main.cpp osx_ios_device_trigger.xcodeproj/project.pbxproj
	xcodebuild -scheme osx_ios_device_trigger TARGET_BUILD_DIR="../../bin"

clean:
	$(RM) $(TARGET)