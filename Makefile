dist: clean_dist
	cd kernel; make dist
	cp -r qemu/dist dist
	cd rust; cargo build || exit; cp target/debug/jjs-kvm ../dist/
	cp python/*.py dist/

clean_dist:
	rm -rf dist

clean: clean_dist
	cd kernel; make clean
	cd qemu; make clean
	rm -rf python/__pycache__
