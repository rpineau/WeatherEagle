#!/bin/bash

PACKAGE_NAME="WeatherEagle_X2.pkg"
BUNDLE_NAME="org.rti-zone.WeatherEagleX2"

if [ ! -z "$app_id_signature" ]; then
    codesign -f -s "$app_id_signature" --verbose ../build/Release/libWeatherEagle.dylib
fi

mkdir -p ROOT/tmp/WeatherEagle_X2/
cp "../weatherstationlist WeatherEagle.txt" ROOT/tmp/WeatherEagle_X2/
cp "../build/Release/libWeatherEagle.dylib" ROOT/tmp/WeatherEagle_X2/
cp "../WeatherEagle.ui" ROOT/tmp/WeatherEagle_X2/

if [ ! -z "$installer_signature" ]; then
	# signed package using env variable installer_signature
	pkgbuild --root ROOT --identifier $BUNDLE_NAME --sign "$installer_signature" --scripts Scripts --version 1.0 $PACKAGE_NAME
	pkgutil --check-signature ./${PACKAGE_NAME}

else
    pkgbuild --root ROOT --identifier $BUNDLE_NAME --scripts Scripts --version 1.0 $PACKAGE_NAME
fi

rm -rf ROOT
