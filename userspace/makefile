MAKEFLAGS += --no-builtin-rules

#first get all the sub directory
apps = $(shell find -name "*" -not -path "./out" -not -path "." -type d)

all :
	$(foreach dir,${apps},make -C ${dir})

clean :
	$(foreach dir,${apps},make -C ${dir} clean)