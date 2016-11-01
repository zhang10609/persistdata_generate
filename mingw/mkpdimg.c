/*
 * Create a Persistent Data Image for Andromeda Box.
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009
 * Jun Yu <yujun@marevell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * mkpdimg.c
 */

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h> 
#include <fcntl.h>

#define TRUE  1
#define FALSE 0

#ifdef MKPDIMAGE_TRACE
#define TRACE(s, args...)	do { \
			              printf("mkpdimage: "s, ## args); \
			        } while(0)
#else
#define TRACE(s, args...)
#endif

#define ERROR(s, args...)       do {\
                                      fprintf(stderr, s, ## args);\
                                } while(0)

#define HELP() \
	printf("mkpdimg version 0.3 (2016/01/14)\n");\
	printf("Author: Jun Yu <yujun@marvell.com>\n"); \
    printf("Usage:mkpdimg <options> -o <output file>\n"); \
    printf("\tOption: -sn <serial number>\n"); \
    printf("\t\tthe board serial number\n"); \
    printf("\tOption: -wifi_mac <xx:xx:xx:xx:xx:xx>\n"); \
    printf("\t\tthe board wifi mac address. The format must be 00:50:43:xx:xx:xx.\n"); \
    printf("\tOption: -wifi_mac_no_check <xx:xx:xx:xx:xx:xx>\n"); \
    printf("\t\tthe board wifi mac address without format checking.\n"); \
    printf("\tOption: -bt_mac <xx:xx:xx:xx:xx:xx>\n"); \
    printf("\t\tthe board bluetooth mac address. The format must be 00:50:43:xx:xx:xx.\n"); \
    printf("\tOption: -bt_mac_no_check <xx:xx:xx:xx:xx:xx>\n"); \
    printf("\t\tthe board bluetooth mac address without format checking.\n"); \
    printf("\tOption: -zb_mac <xx:xx:xx:xx:xx:xx>\n"); \
    printf("\t\tthe board zigbee mac address\n"); \
    printf("\tOption: -o <output file>\n"); \
    printf("\t\tthe path of generated persistent data image file\n"); \
    printf("\tOption: -test_checksum xxx\n"); \
    printf("\t\tto generate the wrong checksum (correct checksum - xxx) for testing purpose only\n"); \
    
#define MAC_ADDRESS_LEN 17 /*xx:xx:xx:xx:xx:xx*/
#define VALID_MARVELL_MAC_1 "00\0"
#define VALID_MARVELL_MAC_2 "50\0"
#define VALID_MARVELL_MAC_3 "43\0"

#define SN_TYPE        0xF001
#define WIFI_MAC_TYPE  0xF002
#define BT_MAC_TYPE    0xF003
#define ZB_MAC_TYPE    0xF004
#define CHECK_SUM_TYPE 0xF0FF

#define BLOCK_NUMBER  4

typedef struct persistent_data_block {
    unsigned int type;
    unsigned int length;
    unsigned char data[0];
}PDATA_BLOCK_t;

static unsigned int image_header = 0x19081400;

static int validateMacAddr(char *mac_addr, int format_checking){
    int index = 1;
    char *delim = ":";
    char *p;
    
    TRACE("mac address is %s\n", mac_addr);
    
    p = strtok(mac_addr, delim);
    TRACE("validateMacAddr(): %d - %s\n", index, p);
    
    if (format_checking){
        if ( strcmp(p, VALID_MARVELL_MAC_1) ){
            ERROR("%s is not valid Marvell mac address(index %d-%s is wrong\n)", mac_addr, index, p);  
            return -1;    
        }
    }
    
    index++;
    while((p = strtok(NULL, delim)) && (index <= 6)){
        TRACE("validateMacAddr(): %d - %s\n", index, p);
        if (format_checking){
            if ( (index == 2) && 
                 strcmp(p, VALID_MARVELL_MAC_2) ){
                ERROR("%s is not valid Marvell mac address(index %d-%s is wrong\n)", mac_addr, index, p); 
                return -1;    
            }
            
            if ( (index == 3) && 
                 strcmp(p, VALID_MARVELL_MAC_3) ){
                ERROR("%s is not valid Marvell mac address(index %d-%s is wrong\n)", mac_addr, index, p); 
                return -1;    
            }
        }
        index++;
    }
    
    if (index == 7)
        return 0;
    else
        return -1;
}

static unsigned int calc_checksum(FILE *fd, unsigned int seed)
{
    unsigned char buf[1];
    while (fread(buf, 1, 1, fd) == 1) {
        seed += buf[0];
    }
    return seed;
}


int main(int argc, char *argv[])
{
    PDATA_BLOCK_t *PDataBlock[BLOCK_NUMBER];
    int i;
    int res;
    int total_blocks = 0;
    int wlen = 0;
    unsigned int checksum = 0;
    PDATA_BLOCK_t *checksum_block;
    FILE *fd = NULL;
    int checksum_offset = 0;
    unsigned int total_data_len = 0;
		char tempbuffer[MAC_ADDRESS_LEN+1];
    
    if(argc > 1 && strcmp(argv[1], "-help") == 0) {
		HELP();
		exit(0);
    }

	//TRACE("The size of the PDATA_BLOCK_t is %d\n",sizeof(PDATA_BLOCK_t));
    memset(PDataBlock, 0, sizeof(PDATA_BLOCK_t *)*BLOCK_NUMBER);
	for(i = 1;	i < argc; i++) {
		if(strcmp(argv[i], "-sn") == 0) {
			if(++i == argc) {
				ERROR("%s: -sn missing serial number\n", argv[0]);
				exit(1);
			}
			
			PDataBlock[total_blocks] = (PDATA_BLOCK_t *)malloc(sizeof(PDATA_BLOCK_t) + strlen(argv[i]));
			if (PDataBlock[total_blocks]){
    			PDataBlock[total_blocks]->type = SN_TYPE;
    			PDataBlock[total_blocks]->length = strlen(argv[i]);
    			total_data_len += PDataBlock[total_blocks]->length + sizeof(PDATA_BLOCK_t);
    			memcpy(PDataBlock[total_blocks]->data, argv[i], PDataBlock[total_blocks]->length);
    			total_blocks++;
    		}else{
    		    ERROR("%s: failed to allocate memory for serial number\n", argv[0]);
    		    exit(1);
    		}
		} else if(strcmp(argv[i], "-wifi_mac") == 0) {
			if(++i == argc) {
				ERROR("%s: -wifi_mac missing wifi mac address\n", argv[0]);
				exit(1);
			}
			strcpy(tempbuffer,argv[i]);
			if (validateMacAddr(tempbuffer, TRUE) != 0){
			    ERROR("%s: the wifi mac address does not comply with Marvell definition(00:50:43:xx:xx:xx)!\n", argv[i]);
				exit(1);
			}
			
			PDataBlock[total_blocks] = (PDATA_BLOCK_t *)malloc(sizeof(PDATA_BLOCK_t) + MAC_ADDRESS_LEN);
			if (PDataBlock[total_blocks]){
    			PDataBlock[total_blocks]->type   = WIFI_MAC_TYPE;
    			PDataBlock[total_blocks]->length = MAC_ADDRESS_LEN;
    			total_data_len += PDataBlock[total_blocks]->length + sizeof(PDATA_BLOCK_t);
    			memcpy(PDataBlock[total_blocks]->data, argv[i], MAC_ADDRESS_LEN);
    			total_blocks++;
    		}else{
    		    ERROR("%s: failed to allocate memory for wifi mac address\n", argv[0]);
    		    exit(1);
    		}
			//TRACE("wifi_mac: %s\n", gPersistentData.wifi_mac);
		} else if(strcmp(argv[i], "-wifi_mac_no_check") == 0){
		    if(++i == argc) {
				ERROR("%s: -wifi_mac missing wifi mac address\n", argv[0]);
				exit(1);
			}
			strcpy(tempbuffer,argv[i]);
			if (validateMacAddr(tempbuffer, FALSE) != 0){
			    ERROR("%s: the wifi mac address is not the standard one(xx:xx:xx:xx:xx:xx)!\n", argv[i]);
				exit(1);
			}
			
			PDataBlock[total_blocks] = (PDATA_BLOCK_t *)malloc(sizeof(PDATA_BLOCK_t) + MAC_ADDRESS_LEN);
			if (PDataBlock[total_blocks]){
    			PDataBlock[total_blocks]->type   = WIFI_MAC_TYPE;
    			PDataBlock[total_blocks]->length = MAC_ADDRESS_LEN;
    			total_data_len += PDataBlock[total_blocks]->length + sizeof(PDATA_BLOCK_t);
    			memcpy(PDataBlock[total_blocks]->data, argv[i], MAC_ADDRESS_LEN);
    			total_blocks++;
    		}else{
    		    ERROR("%s: failed to allocate memory for wifi mac address\n", argv[0]);
    		    exit(1);
    		}
			//TRACE("wifi_mac: %s\n", gPersistentData.wifi_mac);
		}else if(strcmp(argv[i], "-bt_mac") == 0) {
			if(++i == argc) {
				ERROR("%s: -bt_mac missing bluetooth mac address\n", argv[0]);
				exit(1);
			}
			strcpy(tempbuffer,argv[i]);
			if (validateMacAddr(tempbuffer, TRUE) != 0){
			    ERROR("%s: the bluetooth mac address does not comply with Marvell definition(00:50:43:xx:xx:xx)!\n", argv[i]);
				exit(1);
			}
			
			PDataBlock[total_blocks] = (PDATA_BLOCK_t *)malloc(sizeof(PDATA_BLOCK_t) + MAC_ADDRESS_LEN);
			if (PDataBlock[total_blocks]){
    			PDataBlock[total_blocks]->type   = BT_MAC_TYPE;
    			PDataBlock[total_blocks]->length = MAC_ADDRESS_LEN;
    			total_data_len += PDataBlock[total_blocks]->length + sizeof(PDATA_BLOCK_t);
    			memcpy(PDataBlock[total_blocks]->data, argv[i], MAC_ADDRESS_LEN);
    			total_blocks++;
    		}else{
    		    ERROR("%s: failed to allocate memory for wifi mac address\n", argv[0]);
    		    exit(1);
    		}
			//TRACE("bt_mac: %s\n", gPersistentData.bt_mac);
		} else if(strcmp(argv[i], "-bt_mac_no_check") == 0) {
			if(++i == argc) {
				ERROR("%s: -bt_mac missing bluetooth mac address\n", argv[0]);
				exit(1);
			}
			strcpy(tempbuffer,argv[i]);
			if (validateMacAddr(tempbuffer, FALSE) != 0){
			    ERROR("%s: the bluetooth mac address is not the standard one(xx:xx:xx:xx:xx:xx)!\n", argv[i]);
				exit(1);
			}
			
			PDataBlock[total_blocks] = (PDATA_BLOCK_t *)malloc(sizeof(PDATA_BLOCK_t) + MAC_ADDRESS_LEN);
			if (PDataBlock[total_blocks]){
    			PDataBlock[total_blocks]->type   = BT_MAC_TYPE;
    			PDataBlock[total_blocks]->length = MAC_ADDRESS_LEN;
    			total_data_len += PDataBlock[total_blocks]->length + sizeof(PDATA_BLOCK_t);
    			memcpy(PDataBlock[total_blocks]->data, argv[i], MAC_ADDRESS_LEN);
    			total_blocks++;
    		}else{
    		    ERROR("%s: failed to allocate memory for wifi mac address\n", argv[0]);
    		    exit(1);
    		}
			//TRACE("bt_mac: %s\n", gPersistentData.bt_mac);
		} else if(strcmp(argv[i], "-zb_mac") == 0){
			if(++i == argc) {
				ERROR("%s: -zb_mac missing zigbee mac address\n", argv[0]);
				exit(1);
			}
			
			PDataBlock[total_blocks] = (PDATA_BLOCK_t *)malloc(sizeof(PDATA_BLOCK_t) + strlen(argv[i]));
			if (PDataBlock[total_blocks]){
    			PDataBlock[total_blocks]->type   = ZB_MAC_TYPE;
    			PDataBlock[total_blocks]->length = strlen(argv[i]);
    			total_data_len += PDataBlock[total_blocks]->length + sizeof(PDATA_BLOCK_t);
    			memcpy(PDataBlock[total_blocks]->data, argv[i], PDataBlock[total_blocks]->length);
    			total_blocks++;
    		}else{
    		    ERROR("%s: failed to allocate memory for zigbee mac address\n", argv[0]);
    		    exit(1);
    		}
		} else if (strcmp(argv[i], "-o") == 0) {
		    if(++i == argc) {
				ERROR("%s: -o missing output file path\n", argv[0]);
				exit(1);
			}
			
		    fd = fopen(argv[i], "w+");
		    if(fd == NULL) {
                ERROR("Could not open output file <%s>\n", argv[i]);
                exit(1);
            }
		} else if (strcmp(argv[i], "-test_checksum") == 0) {
		    if(++i == argc) {
				ERROR("%s: -o missing test_checksum offset\n", argv[0]);
				exit(1);
			}
			checksum_offset = atoi(argv[i]);
			TRACE("checksum offset %d only for tesing purpose\n", checksum_offset);
		} else {
			ERROR("%s: invalid option\n\n", argv[0]);
			ERROR("SYNTAX:%s -sn <serial number> -wifi_mac <00:50:43:xx:xx:xx> -bt_mac <00:50:43:xx:xx:xx> -zb_mac <xxxx> -o <output file>\n", argv[0]);
			
			exit(1);
		}
	}

	TRACE("Start to write the data blocks\n");
    res = fwrite(&image_header, 1, sizeof(unsigned int), fd);
    if(res < sizeof(unsigned int)) {
    	if(errno != EINTR) {
            ERROR("Write file failed because %s\n", strerror(errno));
    	    return -1;
    	}
    }
	 
    TRACE("write image header 0x%x - done!\n", image_header);
    res = fwrite(&total_data_len, 1, sizeof(unsigned int), fd);
    if(res < sizeof(unsigned int)) {
    	if(errno != EINTR) {
            ERROR("Write file failed because %s\n", strerror(errno));
    	    return -1;
    	}
    }
    TRACE("write total data length %d - done!\n", total_data_len); 
	for (i = 0; i < BLOCK_NUMBER; i++) {
	    if (PDataBlock[i]){
    	    wlen = PDataBlock[i]->length + sizeof(PDATA_BLOCK_t);
    	    res = fwrite(PDataBlock[i], 1, wlen, fd);
            if(res < wlen) {
            	if(errno != EINTR) {
                    ERROR("Write file failed because %s\n", strerror(errno));
            	    return -1;
            	}
            }
            TRACE("write type 0x%x -- done!\n", PDataBlock[i]->type);
        }
	}

    checksum_block = (PDATA_BLOCK_t *)malloc(sizeof(PDATA_BLOCK_t) + sizeof(unsigned int));
    checksum_block->type = CHECK_SUM_TYPE;
    checksum_block->length = sizeof(unsigned int);
    fseek(fd, 0L, SEEK_SET);
    checksum = calc_checksum(fd, 0);
    TRACE("checksum is 0x%x, offset is %d\n", checksum, checksum_offset);
    checksum += checksum_offset;
    memcpy(checksum_block->data, &checksum, checksum_block->length);

    res = fwrite(checksum_block, 1, sizeof(PDATA_BLOCK_t) + sizeof(unsigned int), fd);
    if(res < sizeof(PDATA_BLOCK_t) + sizeof(unsigned int)) {
    	if(errno != EINTR) {
            ERROR("Write file failed because %s\n", strerror(errno));
    	    return -1;
    	}
    }
    
    TRACE("The persistent data image is generated!\n");
       
    if (fd)
        fclose(fd);
    
    for (i = 0; i < BLOCK_NUMBER; i++) { 
        if (PDataBlock[i])
            free(PDataBlock[i]);
    }
    free(checksum_block);
    
    return 0;
}
