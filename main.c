
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


#include "LZMA/LzmaDec.h"
#include "LZMA/LzmaEnc.h"

bool lzma_spd;
FILE *logfile=0;

enum e_action {
    UNPACK=0,
    PACK
};

enum e_compression {
    CMP_UNKNOWN=-1,
    CMP_GZIP=0,
    CMP_LZMA_SPD=1,
    CMP_NONE=2,
    CMP_LZMA_86=3,
    CMP_LZMA=4
};

const char *compression_string[]= {
	"CMP_GZIP",
	"CMP_LZMA_SPD",
	"CMP_NONE",
	"CMP_LZMA_86",
	"CMP_LZMA"
};

static void *SzAlloc(void *p, size_t size) {
	p = p;
	return malloc(size);
}
static void SzFree(void *p, void *address) {
	p = p;
	free(address);
}

typedef struct {
	//unsigned int  magic1;
	//unsigned int  magic2;
	unsigned int imag;
	unsigned int rsso;
	unsigned int bdsp;
	unsigned int code;
	unsigned int EX[24];

} s_download_file_config;

typedef struct {
	unsigned int  magick_BLOC;
	unsigned int  id;
	unsigned int  data_offset;
	unsigned int  packed_size;
	unsigned int  pac_size;
} s_bzpBlockHeader;

typedef struct {
	unsigned int  magic_SPRD;
	unsigned int  type;
	unsigned int  blocks_offset;
	unsigned int  n_blocks;
} s_BzpFileHeader;

typedef struct {
	unsigned int magic_NPAC;
	unsigned int flags;
	unsigned int compressed_data_size;
	unsigned int n_blocks;
	//[data]
	//[data blocks offsets]
} s_npacHeader;

const char *lzma_error_string[]= {
	"SZ_OK",
	"SZ_ERROR_DATA",
	"SZ_ERROR_MEM",
	"SZ_ERROR_CRC",
	"SZ_ERROR_UNSUPPORTED",
	"SZ_ERROR_PARAM",
	"SZ_ERROR_INPUT_EOF",
	"SZ_ERROR_OUTPUT_EOF",
	"SZ_ERROR_READ",
	"SZ_ERROR_WRITE",
	"SZ_ERROR_PROGRESS",
	"SZ_ERROR_FAIL",
	"SZ_ERROR_THREAD",
	"SZ_ERROR_ARCHIVE",
	"SZ_ERROR_NO_ARCHIVE"
};

typedef struct {
	e_compression cmp,rescmp, usrcmp;
	unsigned int level,usrl,usrpacsize,resl,respacsize;
} s_compressionParams;

typedef struct {
	char *filePath;
	unsigned int id;
	unsigned int level,pacsize;
	unsigned int cmp;
} s_bzpBlockParam;

unsigned int getCompressionType(unsigned char *data) {
	if( ((data[0] == 0x5D) || (data[0] == 0x67)) && (data[1] == 0))
		return(CMP_LZMA);
	if((data[0] == 0x5A) && (data[1] == 0))
		return(CMP_LZMA_SPD);
	if((data[0] == 0x1) && ((data[1] == 0x5D) || (data[1] == 0x67)))
		return(CMP_LZMA_86);
	if((data[0] == 0x1F) && (data[1] == 0x8B))
		return(CMP_GZIP);

	return(CMP_NONE);
}


void showProgress(int p100) {
	char temp[64];
	strcpy(temp,"[                                                  ]\r");
	for(int i=1; i<(p100/2+1); i++)
		temp[i]='*';
	printf("%s",temp);

}

bool CopyFile(const char *srcfilePath, const char *destfilePath) {
	char buf[256];
	unsigned int size;
	FILE *in=fopen(srcfilePath,"rb");
	FILE *out=fopen(destfilePath,"wb");

	if(!in || !out) return(1);

	do {
		size=fread(buf,1,sizeof(buf),in);
		size=fwrite(buf,1,size,out);
	} while (size == sizeof(buf));
	fclose(in);
	fclose(out);
	return(0);
}

unsigned int getFileSize(const char *filePath) {
	FILE *f;
	unsigned int result;

	f=fopen(filePath,"rb");
	if(!f)return(0);
	fseek(f,0,SEEK_END);
	result=ftell(f);
	fclose(f);
	return(result);

}
bool isFileExist(const char *name) {
	FILE* f;
	f=fopen(name,"rb");
	if(!f)
		return(0);
	fclose(f);
	return(1);
}


bool lzmaCompress(Byte *dest, SizeT *destLen, const Byte *src, SizeT srcLen, s_bzpBlockParam *bzpBlock) {
	ISzAlloc g_Alloc = { SzAlloc, SzFree };
	CLzmaEncProps props;
	SRes curRes;
	SizeT propsSize=5;

	LzmaEncProps_Init(&props);
	//props.lc = 0;
	//props.pb = 2;
	//props.lp = 0;
	props.level = bzpBlock->level;
	props.fb=32;
	props.mc=32;
	//props.algo=1;
	//props.numHashBytes=4;
	lzma_spd=0;

	*(unsigned long long*)(dest+5)=srcLen;
	curRes = LzmaEncode(dest + 13, destLen,
	                    src, srcLen,
	                    &props, dest, &propsSize , 0,
	                    NULL, &g_Alloc, &g_Alloc);

	if(curRes	!= SZ_OK) {
		printf("LZMA error [%d (%s))].\r\n",curRes, lzma_error_string[curRes]);
		return(1);
	}
	*destLen+=13;
	return(0);
}

bool lzmaSpdCompress(Byte *dest, SizeT *destLen, const Byte *src, SizeT srcLen, s_bzpBlockParam *bzpBlock) {
	ISzAlloc g_Alloc = { SzAlloc, SzFree };
	CLzmaEncProps props;
	SRes curRes;
	SizeT propsSize=5;

	LzmaEncProps_Init(&props);
	props.lc = 0;
	props.pb = 2;
	props.lp = 0;
	props.level = bzpBlock->level;
	props.fb=32;
	props.mc=32;
	lzma_spd=1;

	*(unsigned long long*)(dest+5)=srcLen;
	curRes = LzmaEncode(dest + 13, destLen,
	                    src, srcLen,
	                    &props, dest, &propsSize , 0,
	                    NULL, &g_Alloc, &g_Alloc);

	if(curRes	!= SZ_OK) {
		printf("LZMA error [%d (%s))].\r\n",curRes, lzma_error_string[curRes]);
		return(1);
	}
	*destLen+=13;
	return(0);
}

bool compressFileToFile(FILE *fbzp, char *dataFilePath, s_bzpBlockParam *bzpBlock, unsigned int *compressedDataSize,unsigned int* blocksOffsets) {
	FILE *f;

	unsigned int inFileSize=getFileSize(dataFilePath);
	unsigned int bytesRemaining=inFileSize;
	unsigned char *fileBuf=(unsigned char*)malloc(bzpBlock->pacsize);
	SizeT outSizeProcessed;
	SizeT srcLen;

	unsigned char *tempBuf=(unsigned char *)malloc(bzpBlock->pacsize+4096);
	unsigned int percento=0,blocks_count=0;


	f=fopen(bzpBlock->filePath,"rb");

	printf("Pack file [%s]\r\n",dataFilePath);

	*compressedDataSize=0;

	while(bytesRemaining > 0) {

		memset(	tempBuf,0,bzpBlock->pacsize+4096);

		if(bytesRemaining > bzpBlock->pacsize)
			srcLen=bzpBlock->pacsize;
		else
			srcLen=bytesRemaining;

		outSizeProcessed=bzpBlock->pacsize+4096;

		fread(fileBuf,1,srcLen,f);

		switch((unsigned int)bzpBlock->cmp) {
			case CMP_LZMA_SPD:
				if(lzmaSpdCompress(tempBuf,&outSizeProcessed,fileBuf,srcLen,bzpBlock)) {
					free(tempBuf);
					free(fileBuf);
					fclose(f);
					return(1);
				}
				break;
			case CMP_LZMA:
				if(lzmaCompress(tempBuf,&outSizeProcessed,fileBuf,srcLen,bzpBlock)) {
					free(tempBuf);
					free(fileBuf);
					fclose(f);
					return(1);
				}
				break;
			case CMP_GZIP:
				printf("GZIP compression not yet implemented.\r\n");
				free(tempBuf);
				free(fileBuf);
				fclose(f);
				return(1);
				break;
			case CMP_LZMA_86:
				printf("LZMA86 compression not yet implemented.\r\n");
				free(tempBuf);
				free(fileBuf);
				fclose(f);
				return(1);
				break;
			case CMP_NONE:
				//memcpy(fileBuf,tempBuf,srcLen);
				//outSizeProcessed=srcLen;
				printf("Uncompressed data not yet implemented.\r\n");
				free(tempBuf);
				free(fileBuf);
				fclose(f);
				return(1);
				break;
		}

		if(outSizeProcessed % 4)
			outSizeProcessed+=4-(outSizeProcessed % 4);
		fwrite(tempBuf,1,outSizeProcessed,fbzp);
		*compressedDataSize+=outSizeProcessed;

		bytesRemaining-=srcLen;
		blocks_count++;

		if(blocksOffsets && bytesRemaining)
			blocksOffsets[blocks_count]=blocksOffsets[blocks_count-1]+outSizeProcessed;

		if(percento != (inFileSize-bytesRemaining)*100/inFileSize) {
			percento=(inFileSize-bytesRemaining)*100/inFileSize;
			showProgress(percento);
		}
	}
	free(tempBuf);
	free(fileBuf);
	fclose(f);
	printf("\r\n");
	return(0);
}

bool createBzpArchive(const char *outBzpFileName,
                      unsigned int numBlocks, s_bzpBlockParam *bzpBlocks, bool append) {
	unsigned int bzpHeaderOffset;
	FILE *f=0;
	s_BzpFileHeader bzpfileHeader;
	s_bzpBlockHeader *bzpblockHeader;
	unsigned int block=0;
	unsigned int compressedDataSize;
	unsigned int *blocksOffsets=0;
	unsigned int npacHeaderOffset=0;
	s_npacHeader npacHeader;

	bzpfileHeader.magic_SPRD=0x53505244; //SPRD
	bzpfileHeader.n_blocks=numBlocks;
	bzpfileHeader.type=0;

	if(!numBlocks) {
		printf("createBzpArchive, numBlocks == 0 \r\n");
		return(1);
	}

	if(!bzpBlocks) {
		printf("createBzpArchive, bzpBlocks == NULL \r\n");
		return(1);
	}

	if(append)
		f=fopen(outBzpFileName,"r+b");
	else
		f=fopen(outBzpFileName,"wb");

	if(!f) {
		printf("Can't open file [%s]\r\n",outBzpFileName);
		return(1);
	}

	fseek(f,0,SEEK_END);
	bzpHeaderOffset=ftell(f);

	bzpblockHeader=(s_bzpBlockHeader*)malloc(numBlocks*sizeof(s_bzpBlockHeader));

	fwrite(&bzpfileHeader,1,sizeof(s_BzpFileHeader),f);

	while(block < numBlocks) {
		bzpblockHeader[block].magick_BLOC=0x424C4F43; //BLOC
		bzpblockHeader[block].id=bzpBlocks[block].id;
		bzpblockHeader[block].data_offset=ftell(f)-bzpHeaderOffset;
		bzpblockHeader[block].pac_size=getFileSize(bzpBlocks[block].filePath);

		if(bzpblockHeader[block].pac_size > bzpBlocks[block].pacsize) {
			npacHeader.magic_NPAC=0x4E504143; //NPAC
			npacHeader.flags=0;
			npacHeader.n_blocks=bzpblockHeader[block].pac_size / bzpBlocks[block].pacsize ;
			if(bzpblockHeader[block].pac_size % bzpBlocks[block].pacsize) npacHeader.n_blocks++;
			bzpblockHeader[block].pac_size=bzpBlocks[block].pacsize;
			blocksOffsets=(unsigned int*)malloc(npacHeader.n_blocks*sizeof(int));
			*blocksOffsets=sizeof(s_npacHeader);
			npacHeaderOffset=ftell(f);
			fwrite(&npacHeader,1,sizeof(s_npacHeader),f);

		}

		compressFileToFile(f, bzpBlocks[block].filePath, &bzpBlocks[block],&compressedDataSize,blocksOffsets);

		if(npacHeaderOffset) npacHeader.compressed_data_size=compressedDataSize+sizeof(s_npacHeader);

		if(blocksOffsets) {
			fwrite(blocksOffsets,sizeof(int), npacHeader.n_blocks,f);
			bzpblockHeader[block].packed_size=compressedDataSize+npacHeader.n_blocks*sizeof(int)+sizeof(s_npacHeader);
			free(blocksOffsets);
			blocksOffsets=0;
		} else
			bzpblockHeader[block].packed_size=compressedDataSize;

		if(npacHeaderOffset) {
			fseek(f,npacHeaderOffset,SEEK_SET);
			fwrite(&npacHeader,1,sizeof(s_npacHeader),f);
			fseek(f,0,SEEK_END);
			npacHeaderOffset=0;
		}
		block++;
	}

	/* Write bzp file header */
	bzpfileHeader.blocks_offset=ftell(f)-bzpHeaderOffset;
	fseek(f,bzpHeaderOffset,SEEK_SET);
	fwrite(&bzpfileHeader,1,sizeof(s_BzpFileHeader),f);

	/* Write bzp blocks headers */
	fseek(f,0,SEEK_END);
	fwrite(bzpblockHeader,numBlocks,sizeof(s_bzpBlockHeader),f);

	free(bzpblockHeader);
	fclose(f);

	return(0);
}

bool makeStoneImage(const  char *fileImagePath, char *folderPath, s_compressionParams *params) {
	char temp[260],temp1[260];
	FILE *f;
	s_download_file_config dfc;
	s_bzpBlockParam bzp_sections[2];

	memset(&dfc,0xFF,sizeof(dfc));
#ifdef _WIN32_
	sprintf(temp,"%s\\ps.bin",folderPath);
#else
    sprintf(temp,"%s/ps.bin",folderPath);
#endif
	if(isFileExist(temp))
		CopyFile(temp,fileImagePath);
	else {
		printf("Can't find ps file [%s].\r\n",temp);
		return(1);
	}
#ifdef _WIN32_
	sprintf(temp,"%s\\usr.bin",folderPath);
	sprintf(temp1,"%s\\res.bin",folderPath);
#else
    sprintf(temp,"%s/usr.bin",folderPath);
	sprintf(temp1,"%s/res.bin",folderPath);
#endif

	if(isFileExist(temp) && isFileExist(temp1)) {
		bzp_sections[0].cmp = params->rescmp;
		bzp_sections[0].level=params->resl;
		bzp_sections[0].filePath=temp;
		bzp_sections[0].id=0x75736572; //user
		bzp_sections[0].pacsize=params->respacsize;

		bzp_sections[1].cmp = params->usrcmp;
		bzp_sections[1].level=params->usrl;
		bzp_sections[1].filePath=temp1;
		bzp_sections[1].id=0x7253736F; //rSos
		bzp_sections[1].pacsize=params->usrpacsize;
		dfc.rsso=getFileSize(fileImagePath);
		createBzpArchive(fileImagePath, 2, &bzp_sections[0], 1);

	}
#ifdef _WIN32_
	sprintf(temp,"%s\\kern.bin",folderPath);
#else
    sprintf(temp,"%s/kern.bin",folderPath);
#endif

	if(isFileExist(temp)) {
		bzp_sections[0].cmp = params->cmp;
		bzp_sections[0].level=params->level;
		bzp_sections[0].filePath=temp;
		bzp_sections[0].id=0x494D4147; //IMAG
		bzp_sections[0].pacsize=getFileSize(temp);
		dfc.imag=getFileSize(fileImagePath);
		createBzpArchive(fileImagePath, 1, &bzp_sections[0], 1);
	}

	unsigned int i,write_dfc_ok=0;

	f=fopen(fileImagePath,"r+b");

	while(fread(&i,1,sizeof(int),f))
		if(i == 0x50415254) { //TRAP
			fread(&i,1,sizeof(int),f);
			if(i== 0x494D4147) { //GAMI
				fseek(f,ftell(f),SEEK_SET);
				write_dfc_ok=fwrite(&dfc, 1,sizeof(s_download_file_config),f);
				break;
			}
		}
	fclose(f);

	if(write_dfc_ok) {

		printf("Generate image [%s] complete.\r\n",fileImagePath);
	} else {
		printf("Error. Can't write iamge header. Wrong ps.bin file?\r\n");
	}


	return(0);

}

bool extractDataToFile(char *filePath, unsigned char *block_data, unsigned int pac_size) {
	unsigned char *dest=0;
	s_npacHeader *npac;
	unsigned int lzma_blocks_count=1;
	SizeT unpacked_size;
	ISzAlloc g_Alloc = { SzAlloc, SzFree };
	ELzmaStatus status;
	SRes res;
	SizeT inSizePure;
	FILE *f;
	unsigned int percento=0;
	unsigned int *blocks_off_tbl=0;
	unsigned char *compressed_data;
	unsigned int compression_type;
	//unsigned int bytes_count=pac_size;
	npac=(s_npacHeader*)block_data;

	if (npac->magic_NPAC == *(unsigned int*)"CAPN") {
		lzma_blocks_count=npac->n_blocks;
		blocks_off_tbl=(unsigned int*)(block_data+npac->compressed_data_size);
		//bytes_count=npac->compressed_data_size;
	}

	printf("Extracting [%s]\r\n",filePath);
	f=fopen(filePath,"wb");
	if(!f) {
		printf("Can't create file [%s].\r\n",filePath);
		return(1);
	}
	dest=(unsigned char*)malloc(pac_size);

	if(blocks_off_tbl)
		compressed_data=block_data+blocks_off_tbl[0];
	else
		compressed_data=block_data;

	compression_type = getCompressionType(compressed_data);

	if(logfile) {
		fprintf(logfile,"Extract [%s], copression %s, pacsize = %d\r\n",filePath, compression_string[compression_type], pac_size);
	}

	for(unsigned int i = 0; i < lzma_blocks_count ; i++) {

		if(blocks_off_tbl)
			compressed_data=block_data+blocks_off_tbl[i];
		else
			compressed_data=block_data;

		unpacked_size=*(SizeT*)(compressed_data+5);
		inSizePure = pac_size*2;
		//if(inSizePure > bytes_count)
			//inSizePure = bytes_count;
		lzma_spd=0;
		switch(compression_type) {
			case CMP_LZMA_SPD:
				lzma_spd=1;
			case CMP_LZMA:
				res = LzmaDecode(dest, &unpacked_size, compressed_data + LZMA_PROPS_SIZE + 8, &inSizePure,
				                 compressed_data, LZMA_PROPS_SIZE, LZMA_FINISH_END,
				                 &status, &g_Alloc);
				//inSizePure+=LZMA_PROPS_SIZE+8;

				if(res	!= SZ_OK) {
					printf("LZMA error [%d (%s))].\r\n",res, lzma_error_string[res]);
					free(dest);
					return(1);
				}
				break;

			case CMP_GZIP:
				printf("GZIP decompression not yet implemented.\r\n");
				free(dest);
				fclose(f);
				return(1);
				break;

			case CMP_LZMA_86:
				printf("LZMA86 decompression not yet implemented.\r\n");
				free(dest);
				fclose(f);
				return(1);
				break;

			case CMP_NONE:
				//memcpy(dest,compressed_data,pac_size);
				//unpacked_size=pac_size;
				//inSizePure+=pac_size;
				printf("Uncompressed data not yet implemented.\r\n");
				free(dest);
				fclose(f);
				return(1);
				break;

		}

		fwrite(dest,1,unpacked_size,f);

		if(inSizePure % 4) inSizePure=inSizePure+(4-inSizePure % 4);
		//bytes_count-=inSizePure;

		if(percento != (i+1)*100/lzma_blocks_count) {
			percento=(i+1)*100/lzma_blocks_count;
			showProgress(percento);
		}

	}

	fclose(f);
	free(dest);
	printf("\r\n");
	return(0);
}

bool exctractBZP(char *folder_path,const char *file_name, unsigned char *bzp_data, bool save_bzp) {
	s_BzpFileHeader *bzp=(s_BzpFileHeader*)bzp_data;
	s_bzpBlockHeader *block;
	FILE *f;
	char file_path[260];
	unsigned int bzp_size=bzp->blocks_offset+sizeof(s_bzpBlockHeader)*bzp->n_blocks;


	if((bzp->magic_SPRD != *(unsigned int*)"DRPS") && (bzp->magic_SPRD != *(unsigned int*)"RRPS")) {
		printf("Bzp header error.\r\n");
		return(1);
	}

	for(unsigned int i=0; i<bzp->n_blocks; i++) {
		block=(s_bzpBlockHeader*)&bzp_data[bzp->blocks_offset + i*sizeof(s_bzpBlockHeader)];

		if(block->magick_BLOC != *(unsigned int*)"COLB") {
			printf("Bzp block header error.\r\n");
			return(1);
		}

		if(bzp_size < (block->data_offset + block->packed_size))
			bzp_size=block->data_offset + block->packed_size;

		/* Extract data */
		switch(block->id) {
			case 0x494D4147: //IMAG
#ifdef _WIN32_
				sprintf(file_path,"%s\\kern.bin",folder_path);
#else
                sprintf(file_path,"%s/kern.bin",folder_path);
#endif
				break;
			case 0x75736572: //user
#ifdef _WIN32_
				sprintf(file_path,"%s\\usr.bin",folder_path);
#else
                sprintf(file_path,"%s/usr.bin",folder_path);
#endif
				break;
			case 0x7253736F: //rSos
#ifdef _WIN32_
				sprintf(file_path,"%s\\res.bin",folder_path);
#else
                sprintf(file_path,"%s/res.bin",folder_path);
#endif
				break;
			default:
#ifdef _WIN32_
				sprintf(file_path,"%s\\bzp_%d.bin",folder_path,block->id);
#else
                sprintf(file_path,"%s/bzp_%d.bin",folder_path,block->id);
#endif

		}

		extractDataToFile(file_path,bzp_data + block->data_offset,block->pac_size);

	}

	if(save_bzp) {
#ifdef _WIN32_
		sprintf(file_path,"%s\\%s",folder_path,file_name);
#else
        sprintf(file_path,"%s/%s",folder_path,file_name);
#endif
		f=fopen(file_path,"wb");
		if(!f) {
			printf("Can't create file [%s].\r\n",file_path);
			return(1);
		}

		fwrite(bzp_data,1,bzp_size,f);
		fclose(f);
	}

	return(0);
}

bool splitImageExtract(char *imagePath, char *workingDir, bool save_bzp) {
	FILE *hfile_image,*f;
	unsigned char *image_data;
	int image_size;
	s_download_file_config *dfc;
	unsigned int ps_top_address=-1;
	char temp[260];

	hfile_image=fopen(imagePath,"rb");
	if(!hfile_image) {
		printf("Error! Can't open file [%s]\r\n",imagePath);
		return(1);
	}

	fseek(hfile_image,0,SEEK_END);
	image_size=ftell(hfile_image);
	fseek(hfile_image,0,SEEK_SET);

	if(image_size < 0x10) {
		printf("File [%s] is too small. \r\n",imagePath);
		fclose(hfile_image);
		return(1);
	}

	image_data=(unsigned char*)malloc(image_size);

	fread(image_data,1,image_size,hfile_image);
	fclose(hfile_image);

	dfc=0;

	for(int i=0; i<(image_size-8); i++) {
		if(image_data[i]=='T')
			if(!memcmp(image_data + i,"TRAPGAMI",8)) {
				dfc=(s_download_file_config*)&image_data[i+8];
				break;
			}
	}

	if(!dfc) {
		printf("Can't find resources header (TRAPGAMI) in image [%s]. \r\n",imagePath);
		free(image_data);
		return(1);
	}
#ifdef _WIN32_
	sprintf(temp,"%s\\extract.log",workingDir);
#else
sprintf(temp,"%s/extract.log",workingDir);
#endif

	logfile = fopen(temp,"wb");

	if(dfc->imag != 0xFFFFFFFF)
		if(exctractBZP(workingDir,"IMAG.bzp",&image_data[dfc->imag],save_bzp))
			printf("Extract kernel error.\r\n");
	if(dfc->imag < ps_top_address)
		ps_top_address=dfc->imag;

	if(dfc->rsso != 0xFFFFFFFF)
		if(exctractBZP(workingDir,"RSSO.bzp",&image_data[dfc->rsso],save_bzp))
			printf("Extract resourses error.\r\n");
	if(dfc->rsso < ps_top_address)
		ps_top_address=dfc->rsso;

	if(dfc->bdsp != 0xFFFFFFFF)
		if(exctractBZP(workingDir,"DSP.bzp",&image_data[dfc->rsso],save_bzp))
			printf("Extract DSP error.\r\n");
	if(dfc->bdsp < ps_top_address)
		ps_top_address=dfc->bdsp;

	if(dfc->code != 0xFFFFFFFF)
		if(exctractBZP(workingDir,"CODE.bzp",&image_data[dfc->rsso],save_bzp))
			printf("Extract CODE error.\r\n");
	if(dfc->code < ps_top_address)
		ps_top_address=dfc->code;

	for(int i=0; i<24; i++) {

		if(dfc->EX[i] != 0xFFFFFFFF) {
			sprintf(temp,"EX%02d.bzp",i);
			if(exctractBZP(workingDir,temp,&image_data[dfc->EX[i]],save_bzp))
				printf("Extract EX resource %d error.\r\n",i);
			if(dfc->EX[i] < ps_top_address)
				ps_top_address=dfc->EX[i];
		}
	}

	fclose(logfile);
#ifdef _WIN32_
	sprintf(temp,"%s\\ps.bin",workingDir);
#else
    sprintf(temp,"%s/ps.bin",workingDir);
#endif
	f=fopen(temp,"wb");
	if(!f) {
		printf("Can't create file' [%s]. \r\n",temp);
		return(1);
	}
	fwrite(image_data,1,ps_top_address,f);
	fclose(f);

	free(image_data);
	return(0);
}

void print_usage() {

	printf("Command line options:\n"
	       "bzpwork <stone_image_path>\n"
	       "unpack stone image to the same directory\n"
	       "\n"
	       "bzpwork <stone_image_path> <out_folder_path>\n"
	       "unpack stone image to the out_folder_path\n"
	       "\n"
	       "bzpwork -pack <stone_image_path> <-srcfolder srcfolder> [options]\n"
	       "stone_image_path - path to new stone image\n"
	       "srcfolder - path to folder with ps.bin, res.bin, usr.bin and [kern.bin] files.\n"
	       "options:\n"
	       "[-usrpacsize size]		-	size of user code chunk, default 4096\n"
	       "[-respacsize size]		-	size of resources chunk, default 4096\n"
	       "[-level level]			-	compression level, default 5\n"
	       "[-resl level]			-	compression level for resources, override the -level option for resources.\n"
	       "[-usrl level]			-	compression level for resources, override the -level option for user code.\n"
	       "[-cmp compression]		- 	compression type:[lzmaspd], [lzma].\n"
		   "[gzip], [none], [lzma86] not yet implemented.\n"
	       "Default [lzmaspd]\n"
	       "[-rescmp <compression>]	-	compression type, override the -cmp option for resources\n"
	       "[-usrcmp <compression>]	-	compression type, override the -cmp option for user code\n"
	       "\n"
	       "Examples:\n"
	       "bzpwork stone.bin\n"
	       "bzpwork stone.bin extracted\n"
	       "bzpwork -pack new_stone.bin -srcfolder extracted\n"
	       "bzpwork -pack new_stone.bin -srcfolder extracted -level 9 -usrl 5 -cmp lzmaspd\n");
}

int main(int argc, char ** argv)

{
	char path[260];
	char stoneOrBzpFilePath[260];
	bool save_bzp=0;
	s_compressionParams params;
	e_action action=UNPACK;

	memset(&params,0xFF,sizeof(s_compressionParams));

	path[0]=0;
	/* Parse command line */
	if(argc>1) {
		strcpy(stoneOrBzpFilePath,argv[1]);
		if(argc==3) { //exe stone_path out_folder_path
			strcpy(path,argv[2]);
		} else {
			for(int i = 1; i < argc; i++) {
				if(i != (argc-1)) {
					if(!strcmp(argv[i],"-pack")) {
						strcpy(stoneOrBzpFilePath,argv[++i]);
						action=PACK;
					}
					if(!strcmp(argv[i],"-srcfolder")) {
						strcpy(path,argv[++i]);
					}
					if(!strcmp(argv[i],"-usrpacsize"))
						params.usrpacsize=atoi(argv[++i]);
					if(!strcmp(argv[i],"-respacsize"))
						params.respacsize=atoi(argv[++i]);
					if(!strcmp(argv[i],"-level"))
						params.level=atoi(argv[++i]);
					if(!strcmp(argv[i],"-usrl"))
						params.usrl=atoi(argv[++i]);
					if(!strcmp(argv[i],"-resl"))
						params.resl=atoi(argv[++i]);
					if(!strcmp(argv[i],"-cmp")) {
						i++;
						if(!strcmp(argv[i],"lzma"))
							params.cmp = CMP_LZMA;
						else if(!strcmp(argv[i],"lzmaspd"))
							params.cmp = CMP_LZMA_SPD;
						else if(!strcmp(argv[i],"gzip"))
							params.cmp = CMP_GZIP;
						else if(!strcmp(argv[i],"none"))
							params.cmp = CMP_NONE ;
						else if(!strcmp(argv[i],"lzma86"))
							params.cmp = CMP_LZMA_86 ;
						else {
							printf("Unrecognized paramenter for cmp: [%s]\r\n",argv[i]);
							return(1);
						}
					}
					if(!strcmp(argv[i],"-usrcmp")) {
						i++;
						if(!strcmp(argv[i],"lzma"))
							params.usrcmp = CMP_LZMA;
						else if(!strcmp(argv[i],"lzmaspd"))
							params.usrcmp = CMP_LZMA_SPD;
						else if(!strcmp(argv[i],"gzip"))
							params.usrcmp = CMP_GZIP;
						else if(!strcmp(argv[i],"none"))
							params.usrcmp = CMP_NONE ;
						else if(!strcmp(argv[i],"lzma86"))
							params.usrcmp = CMP_LZMA_86 ;
						else {
							printf("Unrecognized paramenter for usrcmp: [%s]\r\n",argv[i]);
							return(1);
						}
					}
					if(!strcmp(argv[i],"-rescmp")) {
						i++;
						if(!strcmp(argv[i],"lzma"))
							params.rescmp = CMP_LZMA;
						else if(!strcmp(argv[i],"lzmaspd"))
							params.rescmp = CMP_LZMA_SPD;
						else if(!strcmp(argv[i],"gzip"))
							params.rescmp = CMP_GZIP;
						else if(!strcmp(argv[i],"none"))
							params.rescmp = CMP_NONE ;
						else if(!strcmp(argv[i],"lzma86"))
							params.rescmp = CMP_LZMA_86 ;
						else {
							printf("Unrecognized paramenter for rescmp: [%s]\r\n",argv[i]);
							return(1);
						}
					}
				}
				if(!strcmp(argv[i],"-bzp"))
					save_bzp=1;
			}
		}
	} else {
		print_usage();
		return(0);
	}

	/* Normalize params */
	if(params.cmp == CMP_UNKNOWN) params.cmp=CMP_LZMA_SPD;
	if(params.level > 9) params.level = 5;
	if(params.respacsize == (unsigned int)-1)params.respacsize=4096;
	if(params.usrpacsize == (unsigned int)-1)params.usrpacsize=4096;
	if(params.usrl > 9) params.usrl=params.level;
	if(params.resl > 9 )params.resl=params.level;
	if(params.rescmp == CMP_UNKNOWN) params.rescmp=params.cmp;
	if(params.usrcmp == CMP_UNKNOWN) params.usrcmp=params.cmp;

	if(*path == 0) {
		strcpy(path,argv[0]);
#ifdef _WIN32_
		*strrchr(path,'\\')=0;
#else
        *strrchr(path,'/')=0;
#endif
	}

	if(action ==  PACK) {
		if(makeStoneImage(stoneOrBzpFilePath, path, &params)) {
			printf("Error make [%s].",stoneOrBzpFilePath);
			return(1);
		} else {
			printf("Make [%s] ok.",stoneOrBzpFilePath);
			return(0);
		}
	}

	if(!isFileExist(stoneOrBzpFilePath)) {
		printf("Can't open file [%s].",stoneOrBzpFilePath);
		return(1);
	}
    
#ifdef _WIN32_
	mkdir(path);
	#else
    mkdir(path, 0700);
#endif


	//_mkdir(path);


	if(splitImageExtract(stoneOrBzpFilePath,path,save_bzp)) {
		printf("Extract from stone image [%s] error. \r\n",stoneOrBzpFilePath);
		return(1);
	} else
		printf("Extract from stone image [%s] complete. \r\n",stoneOrBzpFilePath);


	return 0;
}

