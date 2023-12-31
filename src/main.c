
// v0.2 - nahrazení zconf.h za stdlib.h
// v0.1

// toto je primitivní example, který využívá struktury ze souboru ext.h
// format - vytvoří layout virtuálního FS s root složkou / a souborem /soubor.txt, který je předán parametrem
// parametry FS jsou schválně nesmyslně malé, aby byl layout jednoduše vidět nástrojem xxd nebo jiným bin. prohlížečem
// read sekce pak přečte binární soubor FS a názorně vypisuje obsah

#include <stdio.h>
#include "ext.h"
#include <string.h>
#include "main.h"

int main(int argc, char **argv) {
    const char *name = argv[1];

    // dojde k zformatovani FS
    if (strcmp("format", argv[2]) == 0 && argc == 3){
        superblock_init();
    }

    // dojde k přečtení obsahu z FS
    else if (strcmp(argv[2], "read") == 0){
        FILE * fptr = fopen(name, "rb");

        if (fptr == NULL){
            exit(2);
        }

        struct superblock sb = {};

        // otisknutí superblock struktury z FS
        fread(&sb, sizeof(sb), 1, fptr);

        // používáme již načtené SB parametry - offsety

        u_char inode_bitmap [sb.bitmap_start_address - sb.bitmapi_start_address];
        u_char data_bitmap [sb.inode_start_address - sb.bitmap_start_address];

        fread(&inode_bitmap, sizeof(inode_bitmap), 1, fptr);
        fread(&data_bitmap, sizeof(data_bitmap), 1, fptr);

        printf("signature: %s\nvolume descriptor: %s\ndisk size: %d\ncluster size: %d\ncluster count: %d\n"
               "bitmapi_start_address: %d\nbitmap_start_address: %d\ninode_start_address: %d\ndata_start_address: %d\n",
                sb.signature, sb.volume_descriptor, sb.disk_size, sb.cluster_size, sb.cluster_count, sb.bitmapi_start_address,
                sb.bitmap_start_address, sb.inode_start_address, sb.data_start_address);

        printf("\nInode bitmapa:\n");
        for (int i = 0 ; i < sizeof(inode_bitmap); i++){
            for (int j=7; j>=0; j--) {
                printf("%d",(inode_bitmap[i] >> j) & 0b1);
            }
        }

        printf("\nData bitmapa:\n");
        for (int i = 0 ; i < sizeof(data_bitmap); i++){
            for (int j=7; j>=0; j--) {
                printf("%d",(data_bitmap[i] >> j) & 0b1);
            }
        }

        // toto je delano natvrdo, ale pro nacteni souborove struktury by mela byt prochazena
        //inode bitmapa po bitech stejně jako je vypisována výše

        printf("\n\nNactene inody:\n");

        struct pseudo_inode files[2] = {};
        fread(&files, sizeof(struct pseudo_inode), 2, fptr);

        for (int i = 0; i < 2; i++) {
            printf("id: %d, isDirectory: %d, references: %d, file_size: %d, direct1: %d, direct2: %d, direct3: %d\n",
                    files[i].nodeid, files[i].isDirectory, files[i].references, files[i].file_size, files[i].direct1,
                    files[i].direct2, files[i].direct3);
        }

        fseek(fptr, sb.data_start_address, SEEK_SET); // přeskočení prázdných inodu rovnou na data

        // cteni obsahu root slozky

        printf("\nObsah root slozky: \n");

        struct directory_item root_folder[2] = {};
        fread(&root_folder, sizeof(struct directory_item), 2, fptr);

        for (int i = 0; i < 2; i++){
            printf("name: '%s',  inode_id: %d\n", root_folder[i].item_name, root_folder[i].inode);
        }

        // cteni dat souboru za využití offsetu na data a přímých odkazů
        // idealni by bylo si upravit inode a prime odkazy dat do pole s nějakou vychozi hodnotou a toto nedelat natvrdo :)

        int indexes[3] = {files[1].direct1, files[1].direct2, files[1].direct3};

        for (int i = 0; i < 3; i++){
            u_char buffer[32] = {0};
            fseek(fptr, sb.data_start_address + indexes[i] * sb.cluster_size, SEEK_SET); // nastaveni pozice na datovy cluster patrici souboru
            fread(&buffer, sb.cluster_size, 1, fptr); // samotne cteni obsahu clusteru do bufferu
            printf("data z clusteru %d: %s\n", indexes[i], buffer);
        }
    }

    return 0;
}
int superblock_init(){
    FILE * fptr = fopen(name, "wb");

        if (fptr == NULL){
           exit(2);
        }

        // zapis superblocku
        struct superblock sb = {};
        fill_default_sb(&sb);
        fwrite(&sb, sizeof(sb), 1, fptr); //zapise superblock na zac souboru
        // automaticky posune fseek

        // vynulovani bitmapy inodes
        u_char inode_bitmap [10] = { 0 };
        inode_bitmap[0] = 0b10000000;

        // vynulovani bitmapy datovych bloku
        u_char data_bitmap [10] = { 0 };
        data_bitmap[0] = 0b10000000;

        // zapis bitmap
        fwrite(&inode_bitmap, sizeof(u_char), 10, fptr);
        fwrite(&data_bitmap, sizeof(u_char), 10, fptr);

        // zapis inodu pro /, velikost nastavena na 32B tedy celý první cluster (kam se vejdou maximálně 2 položky)

        struct pseudo_inode root = {0, true, 1, sb.cluster_size, 0}; // velikost nastavená na maximum jednoho clusteru
        fwrite(&root, sizeof(root), 1, fptr);


        // zapis inodu pro soubor.txt, natvrdo nastavena velikost a dopředu vime kolik odkazů je potřeba - nutné udělat dynamicky
        struct pseudo_inode soubor = {1, false, 1, 74, 1, 3, 5};
        // (1, 3, 5 jsou odkazy na čísla datových bloků - korespenduje s bitmapou)
        fwrite(&soubor, sizeof(soubor), 1, fptr);

        char inode_placeholder [40];
        strcpy(inode_placeholder, "<===========prazdny_inode=============>");
        // na konci řetězce je '\0', proto v xxd výpisu je mezera mezi placeholdery "=>.<="

        for (int i = 0 ; i < 8; i++){
            fwrite(&inode_placeholder, sizeof(inode_placeholder), 1, fptr);
        }

        // data slozky /, prvni je odkaz sam na sebe
        struct directory_item di_root = {0, "."};
        fwrite(&di_root, sizeof(di_root), 1, fptr);
        struct directory_item di_soubor = {1, "soubor.txt"};
        fwrite(&di_soubor, sizeof(di_soubor), 1, fptr);

        // data souboru

        char data_placeholder [32];
        strcpy(data_placeholder, "<============volna_data=======>");

        FILE * fptr1 = fopen(argv[3], "rb");

        if (fptr1 == NULL){
            exit(2);
        }

        unsigned char buffer[32] = {0};
        while ((fread(buffer, 1, sizeof(buffer), fptr1)) > 0){
                fwrite(&buffer, sizeof(buffer), 1, fptr);
                fwrite(&data_placeholder, sizeof(data_placeholder), 1, fptr);
                memset(buffer, 0x00, 32);
        }

        // 3 * zbyle misto pro data
        for (int i = 0 ; i < sb.cluster_count - 1 - 6; i++){
            fwrite(&data_placeholder, sizeof(data_placeholder), 1, fptr);
        }

        fclose(fptr);
        fclose(fptr1);
}