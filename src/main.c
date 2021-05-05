#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
#pragma ide diagnostic ignored "hicpp-signed-bitwise"
#pragma ide diagnostic ignored "readability-non-const-parameter"

#include "main.h"

uint8_t ilog2(uint32_t n) {
    uint8_t i = 0;
    while ((n >>= 1U) != 0)
        i++;
    return i;
}

//--------------------------------------------------------------------------------------------------------
//                                           DEBUT DU CODE
//--------------------------------------------------------------------------------------------------------

/**
 * Exercice 1
 *
 * Prend cluster et retourne son addresse en secteur dans l'archive
 * @param block le block de paramètre du BIOS
 * @param cluster le cluster à convertir en LBA
 * @param first_data_sector le premier secteur de données, donnée par la formule dans le document
 * @return le LBA
 */
uint32_t cluster_to_lba(BPB *block, uint32_t cluster, uint32_t first_data_sector) {
    u_int32_t begin = as_uint32(block->BPB_RsvdSecCnt) + as_uint32(block->BPB_HiddSec) + ((block->BPB_NumFATs) * as_uint32(block->BPB_FATSz32));
    //u_int32_t begin = as_uint32(block->BPB_RsvdSecCnt) + ((block->BPB_NumFATs) * as_uint32(block->BPB_FATSz32));
    return (begin + (cluster - first_data_sector) * block->BPB_SecPerClus);
}

/**
 * Exercice 2
 *
 * Va chercher une valeur dans la cluster chain
 * @param block le block de paramètre du système de fichier
 * @param cluster le cluster qu'on veut aller lire
 * @param value un pointeur ou on retourne la valeur
 * @param archive le fichier de l'archive
 * @return un src d'erreur
 */
error_code get_cluster_chain_value(BPB *block,
                                   uint32_t cluster,
                                   uint32_t *value,
                                   FILE *archive) {

    uint32_t FAT_start = as_uint32(block->BPB_RsvdSecCnt) + as_uint32(block->BPB_HiddSec) + (block->BPB_NumFATs * as_uint32(block->BPB_FATSz32));
    uint32_t logical_address = cluster_to_lba(block, cluster, FAT_start);
    fseek(archive,logical_address,SEEK_SET);
    fread(value,1,4,archive);

    return 0;
}


/**
 * Exercice 3
 *
 * Vérifie si un descripteur de fichier FAT identifie bien fichier avec le nom name
 * @param entry le descripteur de fichier
 * @param name le nom de fichier
 * @return 0 ou 1 (faux ou vrai)
 */
bool file_has_name(FAT_entry *entry, char *name) {
    u_int8_t *name_dir = entry->DIR_Name;
    if(name_dir[0]==0xE5 || name_dir[0]==0x00){
        return false;
    }
    char current_read;
    int counter_name= 0;
    int i = 8;
    u_int8_t current;
    //on check le début (avant l'extension) inspiré de la docu fat32 de microsoft
    while(counter_name<8){
        current_read = name[counter_name];
        if(current_read == '.' || current_read == ' ' || current_read == '\0'){
            if(counter_name==0){
                return false;
            } else {
                break;
            }
        }
        current = (u_int8_t)(toupper(current_read));
        if(current == name_dir[counter_name]){
            counter_name++;
        } else {
            return false;
        }
    }
    // check pour extension
    if(current_read != '\0' && current_read != ' '){
        counter_name++;
        for(i=8; i<11; i++){
            current_read = name[counter_name];
            if(current_read == '\0' || current_read == ' '){
                break;
            }
            current = (u_int8_t)(toupper(current_read));
            if(current == name_dir[i]){
                counter_name++;
            } else {
                return false;
            }
        }
    } else {
        i = counter_name;
    }

    while(i<10){
        u_int8_t value = name_dir[i];
        if(value!=0x20 && value!=0x00){
            return false;
        }
        i++;
    }
    return true;
}

/**
 * Exercice 4
 *
 * Prend un path de la forme "/dossier/dossier2/fichier.ext et retourne la partie
 * correspondante à l'index passé. Le premier '/' est facultatif.
 * @param path l'index passé
 * @param level la partie à retourner (ici, 0 retournerait dossier)
 * @param output la sortie (la string)
 * @return -1 si les arguments sont incorrects, -2 si le path ne contient pas autant de niveaux
 * -3 si out of memory
 */
error_code break_up_path(char *path, uint8_t level, char **output) {
    if(!path || level < 0 || !output){
        return -1;
    }
    char *temp = malloc(sizeof (char) * 64);
    if(!temp){
        return -3;
    }
    int num_levels = 0;
    int i = 0;
    char c = path[i];
    while (c!='\0'){
        if(i!=0){
            if(c == '/'){
                num_levels++;
            }
        }
        i++;
        c = path[i];
    }
    if(num_levels < level){
        return -2;
    }
    i=0;
    int counter = 0;
    int counter_word = 0;
    while (path[i]!='\0'){
        if(path[i] == '/'){
            if(i!=0){
                counter++;
            }
        } else {
            if(counter == level){
                temp[counter_word] = path[i];
                counter_word++;
            }
        }
        if(counter>level){
            break;
        }
        i++;
    }
    char *final = malloc(sizeof (char)*(counter_word+1));
    if(!final){
        return -3;
    }
    for (i=0; i<counter_word; i++){
        final[i] = toupper(temp[i]);
    }
    final[i] = '\0';
    *output = final;
    free(temp);
    return 0;
}


/**
 * Exercice 5
 *
 * Lit le BIOS parameter block
 * @param archive fichier qui correspond à l'archive
 * @param block le block alloué
 * @return un src d'erreur
 */
error_code read_boot_block(FILE *archive, BPB **block) {
    BPB *bpb = malloc (sizeof(BPB));
    if(!bpb){
        return OUT_OF_MEM;
    }
    fseek(archive,0, SEEK_SET);
    fread(bpb->BS_jmpBoot, 1,3,archive);
    fread(bpb->BS_OEMName, 1,8,archive);
    fread(bpb->BPB_BytsPerSec, 1,2, archive);
    fread(&(bpb->BPB_SecPerClus), 1,1, archive);
    fread(bpb->BPB_RsvdSecCnt, 1,2,archive);
    fread(&(bpb->BPB_NumFATs), 1, 1,archive);
    fread(bpb->BPB_RootEntCnt, 1, 2, archive);
    fread(bpb->BPB_TotSec16, 1, 2, archive);
    fread(&(bpb->BPB_Media), 1, 1, archive);
    fread(bpb->BPB_FATSz16, 1, 2, archive);
    fread(bpb->BPB_SecPerTrk, 1, 2, archive);
    fread(bpb->BPB_NumHeads, 1, 2, archive);
    fread(bpb->BPB_HiddSec, 1, 4, archive);
    fread(bpb->BPB_TotSec32, 1, 4, archive);
    fread(bpb->BPB_FATSz32, 1, 4, archive);
    fread(bpb->BPB_ExtFlags, 1, 2, archive);
    fread(bpb->BPB_FSVer, 1, 2, archive);
    fread(bpb->BPB_RootClus, 1, 4, archive);
    fread(bpb->BPB_FSInfo, 1, 2, archive);
    fread(bpb->BPB_BkBootSec, 1, 2, archive);
    fread(bpb->BPB_Reserved, 1, 12, archive);
    fread(&(bpb->BS_DrvNum), 1, 1, archive);
    fread(&(bpb->BS_Reserved1), 1, 1, archive);
    fread(&(bpb->BS_BootSig), 1, 1, archive);
    fread(bpb->BS_VolID, 1, 4, archive);
    fread(bpb->BS_VolLab, 1, 11, archive);
    fread(bpb->BS_FilSysType, 1, 8, archive);
    *block = bpb;
    return 0;
}

/*
 *  checke si un string est valide en tant que filename
 *  */
bool isFile(char *filename){
    int i = 0;
    char c = filename[i];
    while (c!='\0'){
        if(c == '.'){
            return false;
        };
        i++;
        c = filename[i];
    }
    return true;
}

/**
 * Exercice 6
 *
 * Trouve un descripteur de fichier dans l'archive
 * @param archive le descripteur de fichier qui correspond à l'archive
 * @param path le chemin du fichier
 * @param entry l'entrée trouvée
 * @return un src d'erreur
 */
error_code find_file_descriptor(FILE *archive, BPB *block, char *path, FAT_entry **entry) {
    //pour vérifier le path que l'on a obtenu
    int num_levels = 0;
    int i = 0;
    char c = path[i];
    while (c!='\0'){
        if(i!=0){
            if(c == '/'){
                num_levels++;
            }
        }
        i++;
        c = path[i];
    }
    i=0;
    char *name;
//vérifie si on a un fichier dans le string pour le path
    if(num_levels>0){
        while(i<(num_levels-1)){
            break_up_path(path, i, name);
            if(!isFile(name)){
                return -1;
            }
            i++;
        }
    }

    if(!archive || !block || !entry){
        return -1;
    }
    //on commence a partir du premier secteur de données
    u_int32_t begin = as_uint32(block->BPB_RsvdSecCnt) + as_uint32(block->BPB_HiddSec) + ((block->BPB_NumFATs) * as_uint32(block->BPB_FATSz32));
    uint32_t firstClusterAddress = 2;
    uint32_t currentCluster = firstClusterAddress;
    uint32_t current_logical_address;
    uint32_t nextCLuster;
    FAT_entry *temporary_entry =  malloc(sizeof(FAT_entry));
    char **current_Name = malloc(sizeof(char *));
    if(num_levels>0){
        while (i<num_levels){
            break_up_path(path, i, current_Name);
            printf(*current_Name);
            while(!(currentCluster & FAT_EOC_TAG)){
                get_cluster_chain_value(block, currentCluster, &nextCLuster, archive);
                current_logical_address = cluster_to_lba(block, currentCluster, begin);
                fseek(archive,current_logical_address, SEEK_SET);
                fread(temporary_entry->DIR_Name, 1, 11, archive);
                fread(&(temporary_entry->DIR_Attr), 1, 1, archive);
                fread(&(temporary_entry->DIR_NTRes), 1, 1, archive);
                fread(&(temporary_entry->DIR_CrtTimeTenth), 1, 1, archive);
                fread(temporary_entry->DIR_CrtTime, 1, 2, archive);
                fread(temporary_entry->DIR_CrtDate, 1, 2, archive);
                fread(temporary_entry->DIR_LstAccDate, 1, 2, archive);
                fread(temporary_entry->DIR_FstClusHI, 1, 2, archive);
                fread(temporary_entry->DIR_WrtTime, 1, 2, archive);
                fread(temporary_entry->DIR_WrtDate, 1, 2, archive);
                fread(temporary_entry->DIR_FstClusLO, 1, 2, archive);
                fread(temporary_entry->DIR_FileSize, 1, 4, archive);
                if(file_has_name(temporary_entry, *current_Name)){
                    i++;
                    begin = (as_uint32(temporary_entry->DIR_FstClusHI) << 16) + as_uint32(temporary_entry->DIR_FstClusLO);
                    break;
                };
                currentCluster = nextCLuster;
            }
            if(currentCluster & FAT_EOC_TAG){
                return -1;
            }
        }
    } else {
        break_up_path(path, 0, current_Name);
        printf(*current_Name);
        while(!(currentCluster & FAT_EOC_TAG)){
            get_cluster_chain_value(block, currentCluster, &nextCLuster, archive);
            current_logical_address = cluster_to_lba(block, currentCluster, begin);
            fseek(archive,current_logical_address, SEEK_SET);
            fread(temporary_entry->DIR_Name, 1, 11, archive);
            fread(&(temporary_entry->DIR_Attr), 1, 1, archive);
            fread(&(temporary_entry->DIR_NTRes), 1, 1, archive);
            fread(&(temporary_entry->DIR_CrtTimeTenth), 1, 1, archive);
            fread(temporary_entry->DIR_CrtTime, 1, 2, archive);
            fread(temporary_entry->DIR_CrtDate, 1, 2, archive);
            fread(temporary_entry->DIR_LstAccDate, 1, 2, archive);
            fread(temporary_entry->DIR_FstClusHI, 1, 2, archive);
            fread(temporary_entry->DIR_WrtTime, 1, 2, archive);
            fread(temporary_entry->DIR_WrtDate, 1, 2, archive);
            fread(temporary_entry->DIR_FstClusLO, 1, 2, archive);
            fread(temporary_entry->DIR_FileSize, 1, 4, archive);
            if(file_has_name(temporary_entry, *current_Name)){
                break;
            }
            currentCluster = nextCLuster;
        }
        if(currentCluster & FAT_EOC_TAG){
            return -1;
        }
    }
    *entry = temporary_entry;

    return 0;
}

/**
 * Exercice 7
 *
 * Lit un fichier dans une archive FAT
 * @param entry l'entrée du fichier
 * @param buff le buffer ou écrire les données
 * @param max_len la longueur du buffer
 * @return un src d'erreur qui va contenir la longueur des donnés lues
 */
error_code
read_file(FILE *archive, BPB *block, FAT_entry *entry, void *buff, size_t max_len) {
    return 0;
}

// ༽つ۞﹏۞༼つ

int main() {
// ous pouvez ajouter des tests pour les fonctions ici
    char **temp = malloc(sizeof (char *));
    int error = break_up_path("/dossier/dossier2/fichier.ext", 0,temp);
    printf("\nresult = %d", error);
    printf("\ntemp = %s", *temp);
    return 0;
}

// ༽つ۞﹏۞༼つ