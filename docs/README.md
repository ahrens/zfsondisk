This directory contains all files needed
to build the documents.
To generate the final documentations on Linux,
you will have to install [Pandoc](https://pandoc.org/)
and some filters,
[TeX Live](https://tug.org/texlive/) and some packages.
Additionally,
you will have to compile two Pandoc filters written by me.

# Pandoc
It's pretty easy to install Pandoc on Fedora:

```bash
$ sudo dnf install haskell-platform
$ cabal update \
$ cabal install cabal-install \
&& /usr/local/bin/cabal --version \
&& /usr/local/bin/cabal v2-update \
&& /usr/local/bin/cabal v2-install pandoc pandoc-citeproc pandoc-crossref
```

After installation, 
you can remove haskell plat-form to release about 6~7GB disk space.

```bash
$ sudo dnf autoremove -y haskell-platform
```

On Ubuntu,
you can use [stack](https://www.haskellstack.org)
to install Pandoc and the needed filters.

```bash
$ stack install pandoc pandoc-citeproc pandoc-crossref
```

It will likely fail, 
but the solution is in the error message.

Install [Go](https://golang.org), 
clone pandoc-filter from 
https://github.com/vupiggy/pandoc-filter.git,
then

```bash
$ cd codeblaock
$ go build -o codeblock-filter main.go
$ cd ../image
$ go build -o image-filter main.go
```

# Tex Live
To install Tex Live, 
you can use `apt`, `dnf`,
or the install script from CTAN.

```bash
$ wget -c http://mirror.ctan.org/systems/texlive/tlnet/install-tl-unx.tar.gz
&& mkdir ./install-tl \
&& tar --strip-components 1 -zvxf install-tl-unx.tar.gz -C "./install-tl" \
&& ./install-tl/install-tl --profile=./texlive.profile
```

The content of `texlive.profile`: 

```
selected_scheme scheme-small
TEXDIR /usr/local/texlive/2021
TEXMFCONFIG ~/.texlive2021/texmf-config
TEXMFHOME ~/.texmf
TEXMFLOCAL /usr/local/texlive/texmf-local
TEXMFSYSCONFIG /usr/local/texlive/2021/texmf-config
TEXMFSYSVAR /usr/local/texlive/2021/texmf-var
TEXMFVAR ~/.texlive2021/texmf-var
binary_x86_64-linux 1
instopt_adjustpath 1
instopt_adjustrepo 1
instopt_letter 1
instopt_portable 0
instopt_write18_restricted 1
tlpdbopt_autobackup 1
tlpdbopt_backupdir tlpkg/backups
tlpdbopt_create_formats 1
tlpdbopt_desktop_integration 1
tlpdbopt_file_assocs 1
tlpdbopt_generate_updmap 0
tlpdbopt_install_docfiles 1
tlpdbopt_install_srcfiles 1
tlpdbopt_post_code 1
tlpdbopt_sys_bin /usr/bin
tlpdbopt_sys_info /usr/share/info
tlpdbopt_sys_man /usr/share/man
tlpdbopt_w32_multi_user 0

```

Then install some needed LaTeX packages:

```bash
$ sudo tlmgr install \
  standalone \
  luatex85 \
  capt-of \
  tkz-base \
  tkz-euclide \
  numprint \
  xstring \
  pgfopts \
  flowchart \
  makeshape \
  IEEEtran \
  anyfontsize \
  xwatermark \
  framed \
  tocloft \
  catoptions \
  ltxkeys \
  rsfs \
  titlesec \
  diagbox \
  appendix \
  pict2e \
  was \
  fourier \
  utopia \
  listofitems \
  readarray \
  verbatimbox \
  ctex \
  pgfplots \
  enumitem \
  harmony \
  musixtex-fonts \
  adjustbox \
  collectbox \
  siunitx \
  collection-fontsrecommended \
  chngcntr \
  stackengine \
  tasks \
  exam \
  exercise \
  xsim \
  scalerel \
  newpx \
  fontaxes \
  kastrup \
  newtx \
  esvect \
  stix \
  zref \
  tkz-doc \
  mdframed \
  datetime2 \
  tracklang \
  marginnote \
  soulpos \
  soulutf8 \
  needspace \
  footmisc \
  xpatch \
  etoc \
  cancel \
  tikz-3dplot \
  pgf-blur \
  lstaddons \
  xint
```

# Fonts
Running `fc-list` command to check whether the fonts,
"DejaVu Sans" and "Ubuntu Mono" are installed,
if not, install them with `apt` or `dnf`.

# Make

Modify Makefile setting `FILTERDIR` to the cloned and built pandoc-filter directory,
then running `make` in this directory.
You can run `make FILTERDIR=<path/to/pandoc-filter>` to avoid modifying the Makefile.
