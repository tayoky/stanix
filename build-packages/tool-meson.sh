DEPENDENCIES="tool-python"

configure () {
    true
}

build () {
    true
}

install () {
    python3 -m pip install --prefix="$PREFIX" meson ninja mako packaging PyYAML
}