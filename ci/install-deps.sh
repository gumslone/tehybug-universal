#!/usr/bin/env bash
set -euo pipefail

# Installs the arduino-cli toolchain needed to build the TeHyBug firmware.
# Used by CI (.github/workflows/build.yml); can also be run locally — set
# ARDUINO_DIRECTORIES_DATA/DOWNLOADS/USER first if you don't want it to
# touch your regular Arduino setup.
#
# All libraries are vendored in ./libraries (the maintainer's known-good
# versions, including a PubSubClient patched to MQTT_MAX_PACKET_SIZE 4000),
# so only the board core has to be installed.

ESP8266_URL="https://arduino.esp8266.com/stable/package_esp8266com_index.json"

arduino-cli core update-index --additional-urls "$ESP8266_URL"
arduino-cli core install esp8266:esp8266@2.7.4 --additional-urls "$ESP8266_URL"

# Core 2.7.4 does not link precompiled libraries (BSEC's libalgobsec.a);
# override the link recipe to include {compiler.libraries.ldflags}, which
# the BSEC library sets to -lalgobsec via its library.properties.
CORE_DIR="$(arduino-cli config get directories.data)/packages/esp8266/hardware/esp8266/2.7.4"
cat > "$CORE_DIR/platform.local.txt" <<'EOF'
compiler.libraries.ldflags=
recipe.c.combine.pattern="{compiler.path}{compiler.c.elf.cmd}" {build.exception_flags} -Wl,-Map "-Wl,{build.path}/{build.project_name}.map" {compiler.c.elf.flags} {compiler.c.elf.extra_flags} -o "{build.path}/{build.project_name}.elf" -Wl,--start-group {object_files} "{archive_file_path}" {compiler.c.elf.libs} {compiler.libraries.ldflags} -Wl,--end-group  "-L{build.path}"
EOF

echo "==> Toolchain installed"
