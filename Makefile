all:
	make -C ./utils
	make -C ./io
	make -C ./memoria
	make -C ./cpu
	make -C ./kernel

dos2unix:
	find . -type f -name "*.config" -exec dos2unix {} +

clean:
	make clean -C ./utils
	make clean -C ./io
	make clean -C ./memoria
	make clean -C ./cpu
	make clean -C ./kernel