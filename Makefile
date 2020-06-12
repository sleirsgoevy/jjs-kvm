dist: clean_dist
	cd kernel; make dist
	cp -r qemu/dist dist
	cd rust; touch src/launch.rs; cargo build --release || exit; cp target/release/jjs_kvm ../dist/
	cp python/*.py dist/

clean_dist:
	rm -rf dist

clean: clean_dist
	rm -f kernel.bin
	cd kernel; make clean
	cd qemu; make clean
	cd bochs; make clean
	rm -rf python/__pycache__
