PUBDIR	      = .
HTMLSTYLEDIR  = styles
HTMLSTYLES    = $(wildcard $(HTMLSTYLEDIR)/*.css)
HTMLCMDCSS    = $(foreach css, $(HTMLSTYLES), --css $(css))
HTML5TEMPLATE = $(HTMLSTYLEDIR)/tufte.html5
FIGURESDIR    = Figures
include $(FIGURESDIR)/Makefile.fig
FIGURESRC     =	$(foreach fig, $(FIGURES_PDF), $(FIGURESDIR)/src/$(fig:.pdf=.tikz))

PANDOCPATH=/usr/local/bin
FILTERDIR    ?= $(HOME)/Projects/Luke/pandoc-filter
PANDOCFILTERS =	$(FILTERDIR)/codeblock/codeblock-filter	\
		$(FILTERDIR)/image/image-filter		\
		$(PANDOCPATH)/pandoc-crossref		\
		$(PANDOCPATH)/pandoc-citeproc
EXTRAARGS     = $(foreach filter, $(PANDOCFILTERS), -F $(filter))	\
			--listings

CONTAINER ?= f34-doc

ifeq ($(DEBUG), y)
  OUTPUT=--verbose
else
  OUTPUT=>/dev/null 2>&1
endif

doc: zwalk zfs_ondisk_spec

container:
	@podman ps -a | grep $(CONTAINER) >/dev/null 2>&1 || \
	 toolbox create -i $(IMAGE) -c $(CONTAINER)

fig:container
	@toolbox run -c $(CONTAINER) make -C Figures DOCTOOLS=$(DOCTOOLS) 

zwalk: container
	@toolbox run -c $(CONTAINER) make html pdf DEBUG=$(DEBUG)

zfs_ondisk_spec: container
	@toolbox run -c $(CONTAINER) make $@.pdf DEBUG=$(DEBUG)

gen_depend=									\
        $(shell [ "$1" != "" ] && awk '/^(\\input|\\include)\{.*\}$$/		\
                {								\
                    input=gensub(/.*{(.*)}.*/, "\\1", "g");			\
                    if (index(input, ".tex") == 0) {				\
                        if (index(input, "\\") != 1) {				\
                            printf("%s.tex ", input);				\
                        }							\
                    } else {							\
                        if (index(input, "\\") != 1) {				\
                            printf("%s ", input);				\
                        }							\
                    }								\
                }' $1)
zfs_ondisk_spec.pdf: zfs_ondisk_spec.tex $(call gen_depend,zfs_ondisk_spec.tex) $(FIGURESRC)
	@echo "TeXing" $< "3 times..."	
	@set -e; \
	 for i in 1 2 3; do \
		echo "TeXing $$i"; \
		toolbox run -c $(CONTAINER) xelatex $< $(OUTPUT); \
	 done

.SECONDEXPANSION:

zwalk_mdparts = zwalk.md
MDPDFTARGETS  = zwalk.md.pdf
MDHTMLTARGETS = zwalk.md.html

include Makefile.in
