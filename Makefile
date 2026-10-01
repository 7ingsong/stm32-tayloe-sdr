OUTPUT_DIR ?= build

compile: debug

release:
	cmake -DCMAKE_BUILD_TYPE=Release -S . -B ${OUTPUT_DIR}
	cmake --build ${OUTPUT_DIR}

debug:
	cmake -DCMAKE_BUILD_TYPE=Debug -S . -B ${OUTPUT_DIR}
	cmake --build ${OUTPUT_DIR}

upload:
	openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg -c "program build/stm32_cmake.elf verify reset exit"

reset:
	openocd -f interface/stlink-v2.cfg -f target/stm32f1x.cfg -c "init; reset run; exit"

clean:
	rm -rf build

.PHONY: compile release debug upload reset clean