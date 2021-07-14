Pack/unpack Spreadtrum SC65xx stone image.

Command line options:
bzpwork <stone_image_path>
unpack stone image to the same directory

bzpwork <stone_image_path> <out_folder_path>
unpack stone image to the out_folder_path

bzpwork -pack <stone_image_path> <-srcfolder srcfolder> [options]
stone_image_path - path to new stone image
srcfolder - path to folder with ps.bin, res.bin, usr.bin and [kern.bin] files.
options:
[-bzp]
[-usrpacsize size]		-	size of user code chunk, default 4096
[-respacsize size]		-	size of resources chunk, default 4096
[-level level]			-	compression level, default 5
[-resl level]			-	compression level for resources, override the -level option for resources.
[-usrl level]			-	compression level for resources, override the -level option for user code.
[-cmp compression]		- 	compression type:[lzmaspd], [lzma].
[gzip], [none], [lzma86] not yet implemented.
Default lzmaspd
[-rescmp <compression>]	-	compression type, override the -cmp option for resources
[-usrcmp <compression>]	-	compression type, override the -cmp option for user code

Examples:
bzpwork stone.bin
bzpwork stone.bin extracted
bzpwork -pack new_stone.bin -srcfolder extracted
bzpwork -pack new_stone.bin -srcfolder extracted -level 9 -usrl 5 -cmp lzmaspd


