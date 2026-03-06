.PHONY: test format upload monitor

test:
	pio test -e native

format:
	clang-format -i src/*.cpp src/*.h

upload:
	pio run -e esp32doit-devkit-v1 -t upload

monitor:
	pio device monitor
