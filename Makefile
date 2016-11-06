OUT=bin
SRC=src

all: $(OUT) $(OUT)/aucont_start $(OUT)/aucont_stop $(OUT)/aucont_list $(OUT)/aucont_exec

${OUT}:
	mkdir -p ${OUT}

$(OUT)/%.o: $(SRC)/%.cpp
	g++ -c -MMD -o $@ $<

$(OUT)/aucont_start: $(OUT)/aucont_start.o $(OUT)/aucont_common.o
	g++ $(OUT)/aucont_start.o $(OUT)/aucont_common.o -o $(OUT)/aucont_start

$(OUT)/aucont_stop: $(OUT)/aucont_stop.o $(OUT)/aucont_common.o
	g++ $(OUT)/aucont_stop.o $(OUT)/aucont_common.o -o $(OUT)/aucont_stop

$(OUT)/aucont_list: $(OUT)/aucont_list.o $(OUT)/aucont_common.o
	g++ $(OUT)/aucont_list.o $(OUT)/aucont_common.o -o $(OUT)/aucont_list

$(OUT)/aucont_exec: $(OUT)/aucont_exec.o $(OUT)/aucont_common.o
	g++ $(OUT)/aucont_exec.o $(OUT)/aucont_common.o -o $(OUT)/aucont_exec

clean:
	rm -rf $(OUT)

include $(wildcard $(OUT)/*.d)

.PHONY: clean all
