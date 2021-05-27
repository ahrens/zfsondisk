ifeq ($(DEBUG), y)
  OUTPUT=--verbose
else
  OUTPUT=>/dev/null 2>&1
endif

include Makefile.fig
FIGURES_SVG =   $(patsubst %.pdf,%.svg,$(FIGURES_PDF))

%.pdf: src/%.tikz
	@printf "%-12s %s\n" "Tikzing " $<
	@tikzfile=$<; texfile=$${tikzfile%%.tikz}.tex; tikzfile=`basename $<`;		\
	 printf "\documentclass[border=2pt]{standalone}\n\n"		>$${texfile};	\
	 printf "\input{colors.tex}\n"					>>$${texfile};	\
	 printf "\input{zfs_tikz.tex}\n"				>>$${texfile};	\
	 printf "\\\begin{document}\n"					>>$${texfile};	\
	 printf "\input{$${tikzfile}}\n"				>>$${texfile};	\
	 printf "\\\end{document}\n\n"					>>$${texfile};	\
	 TEXINPUTS=$(TEXINPUTS):./src							\
	 xelatex -interaction=nonstopmode -output-directory=./ $${texfile} $(OUTPUT)

%.svg: %.pdf
	@printf "%-12s %-20s to SVG\n" "Exporting" "$^"
	@pdf2svg $< $@

figures: $(FIGURES_PDF) $(FIGURES_SVG)
