# Scurtături pentru dezvoltare, ca „npm run …” la proiectele web.
# Pentru build-ul optimizat: make run CONFIG=release

CONFIG ?= debug

.PHONY: setup build test run clean reconfigure

setup:        ## Instalează uneltele necesare (o singură dată)
	@scripts/setup.sh

build:        ## Compilează
	@scripts/dev.sh build $(CONFIG)

test:         ## Compilează și rulează testele
	@scripts/dev.sh test $(CONFIG)

run:          ## Compilează și pornește aplicația
	@scripts/dev.sh run $(CONFIG)

clean:        ## Șterge folderul de build
	@scripts/dev.sh clean $(CONFIG)

reconfigure:  ## Reface configurarea CMake (după modificări în CMakeLists sau preseturi)
	@scripts/dev.sh reconfigure $(CONFIG)
