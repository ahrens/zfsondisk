PUBDIR	      = .
HTMLSTYLEDIR  = styles
HTMLSTYLES    = $(wildcard $(HTMLSTYLEDIR)/*.css)
HTMLCMDCSS    = $(foreach css, $(HTMLSTYLES), --css $(css))
HTML5TEMPLATE = $(HTMLSTYLEDIR)/tufte.html5
FIGURESDIR    = Figures
include $(FIGURESDIR)/Makefile.fig
FIGURESRC     =	$(foreach fig, $(FIGURES_PDF), $(FIGURESDIR)/src/$(fig:.pdf=.tikz))

PANDOCPATH=$(HOME)/.local/bin
FILTERDIR    ?= $(HOME)/Projects/Luke/pandoc-filter
PANDOCFILTERS =	$(FILTERDIR)/codeblock/codeblock-filter	\
		$(FILTERDIR)/image/image-filter		\
		$(PANDOCPATH)/pandoc-crossref $(PANDOCPATH)/pandoc-citeproc
EXTRAARGS     = $(foreach filter, $(PANDOCFILTERS), -F $(filter))	\
			--listings

ifeq ($(DEBUG), y)
  OUTPUT=--verbose
else
  OUTPUT=>/dev/null 2>&1
endif

.SECONDEXPANSION:

zfs_internals_mdparts = zfs_internals.md
MDPDFTARGETS  = zfs_internals.md.pdf
MDHTMLTARGETS = zfs_internals.md.html

include Makefile.in
