PUBDIR	      = .
HTMLSTYLEDIR  = styles
HTMLSTYLES    = $(wildcard $(HTMLSTYLEDIR)/*.css)
HTMLCMDCSS    = $(foreach css, $(HTMLSTYLES), --css $(css))
HTML5TEMPLATE = $(HTMLSTYLEDIR)/tufte.html5
FIGURESDIR    = Figures
include $(FIGURESDIR)/Makefile.fig
FIGURESRC     =	$(foreach fig, $(FIGURES_PDF), $(FIGURESDIR)/src/$(fig:.pdf=.tikz))

#PANDOCPATH=$(HOME)/.local/bin
PANDOCPATH=/usr/local/bin
FILTERDIR    ?= $(HOME)/Projects/Luke/pandoc-filter
PANDOCFILTERS =	$(FILTERDIR)/codeblock/codeblock-filter	\
		$(FILTERDIR)/image/image-filter		\
		$(PANDOCPATH)/pandoc-crossref		\
		$(PANDOCPATH)/pandoc-citeproc
EXTRAARGS     = $(foreach filter, $(PANDOCFILTERS), -F $(filter))	\
			--listings

TEXTRA = zfs_ondisk_cover.tex templates/preamble.tex
CONTAINER ?= f34-doc

container:
	@podman ps -a | grep $(CONTAINER) >/dev/null 2>&1 || \
	 toolbox create -i $(IMAGE) -c $(CONTAINER)

fig:container
	@toolbox run -c $(CONTAINER) make -C Figures DOCTOOLS=$(DOCTOOLS) 

doc:container zfs_ondisk_cover.tex
	@toolbox run -c $(CONTAINER) make html pdf DEBUG=$(DEBUG)

ifeq ($(DEBUG), y)
  OUTPUT=--verbose
else
  OUTPUT=>/dev/null 2>&1
endif

.SECONDEXPANSION:

zfs_internals_mdparts = zfs_internals.md
zfs_ondisk_draft_mdparts = zfs_ondisk_draft.md zfs_ondisk_vdev.md zfs_ondisk_blkptr.md \
			   zfs_ondisk_dmu.md zfs_ondisk_dsl.md

MDPDFTARGETS  = zfs_internals.md.pdf zfs_ondisk_draft.md.pdf
MDHTMLTARGETS = zfs_internals.md.html zfs_ondisk_draft.md.html

include Makefile.in
