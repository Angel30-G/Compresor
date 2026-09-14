# Compresor Huffman

Proyecto desarrollado en C para implementar compresion y descompresion de archivos utilizando el algoritmo de Huffman.

## Funcionalidades implementadas

- Conteo de frecuencias de bytes.
- Construccion de min-heap.
- Construccion del arbol Huffman.
- Generacion de codigos Huffman.
- Compresion de archivos.
- Descompresion de archivos.
- Almacenamiento del MD5 original dentro del archivo comprimido.
- Verificacion de integridad despues de la descompresion.
- Manejo de archivo vacio.
- Manejo de archivo con un unico simbolo.
- Pruebas con archivos de texto reales de Project Gutenberg.

## Formato del archivo .huff

El archivo comprimido contiene:

- Firma `HUF1`.
- Tamano original del archivo.
- MD5 del archivo original.
- Tabla de frecuencias de 256 bytes.
- Datos comprimidos mediante Huffman.

## Compilar

```bash
make clean
make
./bin/gui
