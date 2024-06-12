.RECIPEPREFIX := $() $()

SUBDIRS := stage1 stage2

.PHONY: all $(SUBDIRS)

all clean build: $(SUBDIRS)

$(SUBDIRS):
    $(MAKE) -C $@ $(MAKECMDGOALS)
