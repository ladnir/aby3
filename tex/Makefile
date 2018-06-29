MAIN = main
PDF = pdflatex
BIB = bibtex
BIBOPTS = -min-crossrefs=10

all: $(MAIN).pdf

dist: main.pdf
	rm -r dist
	rm submission.tgz
	mkdir -p dist
	cp *.tex dist
	cp *.bib dist
	cp main.pdf dist
	tar czf submission.tgz dist 

$(MAIN).pdf: *.tex *.bib
	$(PDF) $(MAIN) && $(BIB) $(BIBOPTS) $(MAIN) && $(PDF) $(MAIN) && $(PDF) $(MAIN)

quick: *.tex
	$(PDF) $(MAIN)

clean:
	rm -f *bbl *log *out *aux *blg $(MAIN).pdf

setupbib:
	git submodule add https://github.com/cryptobib/export cryptobib
	( cd cryptobib && git pull origin master )
	git submodule update --init


