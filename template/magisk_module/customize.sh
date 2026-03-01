#!/system/bin/sh

SKIPMOUNT=false
PROPFILE=false
POSTFSDATA=false
LATESTARTSERVICE=false

CONFIG_DIR="/data/local/tmp/.il2cppdump"
CONFIG_FILE="$CONFIG_DIR/config.txt"

ui_print "- Preparing il2cppdump config"
mkdir -p "$CONFIG_DIR"

if [ ! -f "$CONFIG_FILE" ]; then
  cat > "$CONFIG_FILE" <<'EOF'
# Zygisk-Il2CppDumper config
# package_name: target game package
# enable: 1 to enable dump, 0 to disable dump
package_name=com.game.packagename
enable=0
EOF
  chmod 0644 "$CONFIG_FILE"
  ui_print "- Created template: $CONFIG_FILE"
else
  ui_print "- Keep existing config: $CONFIG_FILE"
fi
