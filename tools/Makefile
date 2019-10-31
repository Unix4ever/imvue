env: requirements.txt
	rm -rf env
	virtualenv env
	env/bin/pip install -r requirements.txt

bindings: env
	env/bin/python generate.py -f ../imgui/imgui.h

all: bindings

.PHONY: bindings
