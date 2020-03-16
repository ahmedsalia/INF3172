/*******************************************************************************
 
AUTOMD2H
 
  le program permet de convertir les fichiers au format Markdown en fichiers 
  au format HTML..
 
  SYNOPSIS         : automd2h -options fichier/repertoire 
   
  OPTIONS           :-n :   L'option -n désactive l'utilisation de pandoc,
                            à la place, la liste des chemins des fichiers
                            sources à convertir sera affichée (un par ligne).
                    -t  :   Avec l'option -t, la date de dernière modification 
                            des fichiers est utilisée pour savoir s'il faut 
                            reconvertir. Si le fichier source est plus récent 
                            que le ficher .html cible associé, 
                            ou si le fichier .html cible n'existe pas, alors 
                            il y a conversion.Si la date est identique ou si le 
                            fichier .html cible est plus récent, 
                            alors il n'y a pas de conversion.

                    -r :    L'option -r visite les répertoires récursivement et 
                            cherche les fichiers dont l'extension est .md
                            pour les convertir. 

                    -w :    Avec l'option -w, automd2h bloque et surveille 
                            les modifications des fichiers et des répertoires 
                            passés en argument. Lors de la modification d'un 
                            fichier source, celui-ci est automatiquement 
                            reconverti.Si dans un répertoire surveillé 
                            un fichier .md apparait, est modifié, est déplacé 
                            ou est renommé, celui-ci aussi 
                            est automatiquement converti.
			        
			        -f :    Combiné avec -w, l'option -f force la conversion
		                    immédiate des fichiers trouvés puis surveille
		                    les modifications futures.

 
  
  ERREUR           : Les options envoye n'existe pas ou sont incorrecte.
                     Nombre de paramètres incorrecte
					 Le ficher/repertoire à convertir n'existe pas ou n'est 
					 pas accessible
					 
 
  AUTEUR           : Ahmed Salia Toure TOUA01119806 
                     Lounis Adjissa    ADJL20069504
 
 
*******************************************************************************/

#include<sys/wait.h>
#include<stdbool.h>
#include<stdlib.h>
#include<stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <utime.h>
#include <fcntl.h>
#include <string.h>
#include <sys/inotify.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>


#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define EVENT_BUF_LEN     ( 1024 * ( EVENT_SIZE + 16 ) )

struct EventMap
{
  int id;
  char* nom;
};

char* rechercheNom(int nbNoms, struct EventMap map[50], int wd);
int verifOption(int argc,char *argv[], bool* n, bool* t, bool* w, bool* f, bool* r);
void to_HTML(const char* nom, char**res);
bool is_MD(const char*file);
bool is_TXT(const char*file);
int noOption(const char* file);
bool optionT(const char* file);
int optionN(const char* file);
bool isDirectory(const char* dir);
bool isSymLink(const char* file);
void listdir(const char *name,const bool t,const bool n,int* ret);



int main(int argc, char *argv[]) {
	bool n = false; // Option -n
	bool pandoc = true; // Pour savoir si il faut convertir ou non
	bool t = false; // Option -t
	bool w = false; // Option -w 
	bool f = false; // Option -f
	bool r = false; // Option -r
	int status; 
	int retour = 0 ;
	// Verification des Options.    
	int option = verifOption(argc, argv, &n, &t, &w, &f, &r);
	int i; 
	char *argv2[argc+1];
	argv2[0] = argv[0];
	argv2[1] = "-o";
	bool aConvertir[argc+1] ;
	for( i = 0 ; i  < argc+1 ; i++)
		aConvertir[i] = true;
		
	for( i = 2 ; i < argc-option+1 ; i++)
		argv2[i] = argv[option+i-1];
		
	if (r == true ) {
		pandoc = false;
		for (int z = 2 ; z < argc-option+1; z++){
			 if(isDirectory(argv2[z])){
				listdir(argv2[z],t,n,&retour);
			 }else if (isSymLink(argv2[z])) {
                                struct stat file_stat;
               		        char* file;
                                ssize_t r;
                                if(lstat(argv2[z],&file_stat) == -1){
                                        retour = 1;
                                        continue;
                                }
                                file = malloc(file_stat.st_size + 1);
                                if(file == NULL){
                                        retour = 1; 
                                        continue; 
                                }
                                r = readlink(argv2[z], file, file_stat.st_size + 1);
                                if(r == -1){
                                        retour = 1;
                                        continue;
                                }
                                if(r > file_stat.st_size){
                                        retour = 1 ;
                                        continue;
                                }
                                file[r] = '\0';
                                if(t){
                                      	if(optionT(file)){
						if(!n)
                                                	retour = noOption(file);
                                 		else
							retour = optionN(file);
				        }
                                }else{
					  if(!n)
                                                retour = noOption(file);
                                        else
                                                retour = optionN(file);
                                } 
			 }else{
				if(t){
					if(optionT(argv2[z])){
						if(n)
							retour = optionN(argv2[z]);
						else
							retour = noOption(argv2[z]);
					}
				}else 
					if(n)
						retour = optionN(argv2[z]);
					else
						retour = noOption(argv2[z]);
                	}

		}
		return retour;
	 }

	if(t == true && r == false){
		for (int z = 2 ; z < argc-option+1; z++){
			if(!isDirectory(argv2[z]))
				aConvertir[z] = optionT(argv2[z]);
		}
	}


	if (w == true){
		int fd,wd,nbObjets = 0;
		int nbEvents = 0;
		size_t R;
		int count;
		fd_set fds;
		char buffer[EVENT_BUF_LEN];
		struct inotify_event *event;
		struct EventMap map[50];
		/* Initialisation d'inotify */
		fd = inotify_init();
		if (fd < 0) {
			perror("inotify_init");
			return EXIT_FAILURE;
		}

		//Ajout des fichiers et/ou repertoires passes en parametres
		for(int z = option + 1 ; z < argc; z++){ 
			wd = inotify_add_watch(fd, argv[z], IN_ALL_EVENTS);  
			map[nbObjets].id = wd;
			map[nbObjets].nom = argv[z];
			nbObjets++;
			if (wd < 0) {
				perror("inotify_add_watch");
				return EXIT_FAILURE;
			}
		}

		if (f == true) {
			if (t == true){ //-w -t -f
				//Il faut acceder aux fichier dans le repertoire et pas passer les repertoires en parametre 
				for (int z = 0; z < nbObjets; z++){
					if(!isDirectory(map[z].nom)){ 
						if(optionT(map[z].nom)){
							retour = noOption(map[z].nom);
						}
				          }else{ //Repertoire -w -t -f
						char* nomDir = map[z].nom;
						DIR *dir = opendir(nomDir);
						struct dirent* entry;
						if(dir != NULL){
							while(entry = readdir(dir)){
								if(entry->d_name[0] == '.') continue;
								if(is_MD(entry->d_name)){
									char file[1024];
									snprintf(file, sizeof(file), "%s/%s", nomDir, entry->d_name);
									if(optionT(file))
										retour = noOption(file);
								}
							}
							closedir(dir);
						}else{
							perror("failed to open dir");
						}
					}
				}
      			} else {//-w -f
        			for (int z = 0; z < nbObjets; z++){
					char* nomFichier = map[z].nom;
					//est-ce un fichier ".md" ? 
					if(is_MD(nomFichier)){
						retour = noOption(nomFichier);
					//S'agit-il d'un fichier .txt ?
					} else if (is_TXT(nomFichier)){
						retour = noOption(nomFichier);
				}
			}
		}
	}
	DIR *dir;
	while (1) {
		/* Obtention des informations sur l'evenement qui vient de se produire */
		if(nbEvents == 0 && n == false){
			dir = opendir("d");
		}
		R = read(fd, buffer, EVENT_BUF_LEN);
		if (R <= 0) {
			perror("read");
			return EXIT_FAILURE;
		}
		event = (struct inotify_event *) buffer;
		nbEvents++;
		//Effectuer les bons traitements selon les cas
		//Creation dans un repertoire surveillé
		if (event->mask & IN_CREATE || event->mask & IN_CLOSE_WRITE || event->mask & IN_MOVED_TO) {
			//verifier si le fichier est .md
			char* nomObjet = event->name; 
			//est-ce un fichier ".md" ? dans ce cas il s'agit d'un evenement cree dans un repertoire surveillé
			if(is_MD(nomObjet)){
				char* out1;
				char* dir;
				//obtenir le repertoire courant
				dir = rechercheNom(nbObjets, map, event->wd);
				char file[1024];
				snprintf(file, sizeof(file), "%s/%s", dir, nomObjet);
				if (n == false){
					retour = noOption(file);
				} else{
					fflush(stdout);
					retour = optionN(file);
				}
				//Un fichier texte dans un repertoire surveille
			} else { //Il ne s'agit pas d'un repertoire surveille mais d'un fichier en particulier{
					char* nomFichier = rechercheNom(nbObjets, map, event->wd);
					if(is_MD(nomFichier)){
						if (n == false){
							retour = noOption(nomFichier);
						} else{
							fflush(stdout);
							retour = optionN(nomFichier);
						}
						//S'agit-il d'un fichier .txt ?
					} else if (is_TXT(nomFichier)){
						if (n == false){
							retour = noOption(nomFichier);
						} else{
							fflush(stdout);
							retour = optionN(nomFichier);
						}
					}
				}
			} else if (event->mask & IN_MOVED_FROM){
				bool trouve = false;
				char* nomObjet;
				int wdFichier;
				//L'evenement MOVED_IN et MOVED_FROM partagent le meme cookie 
				for(int b = 0; b < nbEvents; b++){
					if(event->cookie == event[b].cookie && event->name != event[b].name){  
						nomObjet = event[b].name;
						trouve = true;
						wdFichier = event[b].wd;
					}
				}
				if(trouve){
					if(is_MD(nomObjet)){
						char* out1;
						char* dir;
						dir = rechercheNom(nbObjets, map, wdFichier);
						char file[1024];
						snprintf(file, sizeof(file), "%s/%s", dir, nomObjet);
						if (n == false){
							retour = noOption(file);
						} else{
							fflush(stdout);
							retour = optionN(file);
						}
					}
				}
			}
			if(nbEvents == 1 && n == false)
				closedir(dir);
			fflush(stdout);  
		}
		return EXIT_FAILURE;
	}

	if(n == true && w == false && r == false){
		pandoc = false;
		for (int z = 2 ; z < argc-option+1; z++){
		if(isDirectory(argv2[z])){
			DIR *dir = opendir(argv2[z]);
			struct dirent* entry;
			if(dir != NULL){
				while(entry = readdir(dir)){
					if(entry->d_name[0] == '.') continue;
					if(is_MD(entry->d_name)){
						char file[1024];
						snprintf(file, sizeof(file), "%s/%s", argv2[z], entry->d_name);
						if(t){
							if(optionT(file))
								retour = optionN(file);
						}else
							retour = optionN(file);
						}
					}
					closedir(dir);
				}else{
					perror("failed to open dir");
				}
			 }else if (isSymLink(argv2[z])) {
                                        struct stat file_stat;
                                        char* file;
                                        ssize_t r;
                                        if(lstat(argv2[z],&file_stat) == -1){
                                                retour = 1;
                                                continue;
                                        }
                                        file = malloc(file_stat.st_size + 1);
                                        if(file == NULL){
                                                retour = 1; 
                                                continue; 
                                        }
                                        r = readlink(argv2[z], file, file_stat.st_size + 1);
                                        if(r == -1){
                                                retour = 1;
                                                continue;
                                        }
                                        if(r > file_stat.st_size){
                                                retour = 1 ;
                                                continue;
                                        }
                                        file[r] = '\0';
                                        if(t){
                                              	if(optionT(file)){
                                                        retour = optionN(file);
                                                 }
                                        }else{
                                                retour = optionN(file);
                                        } 
			}else{ 
				if(aConvertir[z]){ 
					retour = optionN(argv2[z]);
				}
			}
		}
		return retour;
	} 

		for (int z = 2 ; z < argc-option+1; z++){
			if(isDirectory(argv2[z])){
				DIR *dir = opendir(argv2[z]);
				struct dirent* entry;
				if(dir != NULL){
					while(entry = readdir(dir)){
						if(entry->d_name[0] == '.') continue;
						if(is_MD(entry->d_name)){
							char file[1024];
							snprintf(file, sizeof(file), "%s/%s", argv2[z], entry->d_name);
							if(t){
								if(optionT(file))
									retour = noOption(file);
								} else
									retour = noOption(file);
							}
						}
					closedir(dir);
			        }else{
					perror("failed to open dir");
				}
			}else if (isSymLink(argv2[z])) {
					struct stat file_stat;
					char* file;
					ssize_t r;
					if(lstat(argv2[z],&file_stat) == -1){
						retour = 1;
						continue;
					}
					file = malloc(file_stat.st_size + 1);
					if(file == NULL){
						retour = 1; 
						continue; 
					}
					r = readlink(argv2[z], file, file_stat.st_size + 1);
					if(r == -1){
						retour = 1;
						continue;
					}
					if(r > file_stat.st_size){
						retour = 1 ;
						continue;
					}
					file[r] = '\0';
					if(t){
                                                if(optionT(file)){
                                                        retour = noOption(file);
                                                 }
					}else{
                                              	retour = noOption(file);
					}	
			}else{ 
				if(aConvertir[z])
					retour = noOption(argv2[z]);
			}
		}
  	return retour;
}

char* rechercheNom(int nbNoms, struct EventMap map[50], int wd){
	int i = 0;
	char* retour = " ";
	while(i < nbNoms){
		if(map[i].id == wd){
			retour = map[i].nom;
		}
	i++;
	}
 	 return retour;
} 
/*
* Verification des options passer en parametre
* 
* @param    argc    Le nombre d'argument
* @param    argv    Le tableau contenant les parametre 
* @param    n       Indique la présence de l'option n 
* @param    t       Indique la présence de l'option t
* @param    w       Indique la présence de l'option w
* @param    f       Indique la présence de l'option f
* @param    r       Indique la présence de l'option r
*
* @return  Le nombre d'option passer en paramètre.
*
*/

int verifOption(int argc,char *argv[], bool* n, bool* t, bool* w, bool* f, bool* r){
	int z;
	int option = 0;  
	for( z = 0; z < argc; z++){
		if(strcmp(argv[z], "-n") == 0){
			*n = true;
			option++;
		}
		if(strcmp(argv[z], "-t") == 0){
			*t = true;
			option++;       
		}
		if(strcmp(argv[z], "-w") == 0){
			*w = true;
			option++;       
		}
		if(strcmp(argv[z], "-f") == 0){
			*f = true;
			option++;       
		}
		if(strcmp(argv[z], "-r") == 0){
			*r = true;
			option++;       
		}
	}
	return option;
}

/*
* Ajoute l'extension .html au nom de fichier
*
* @param nom le nom du fichier d'origine.
* @param res un nouveau nom de fichier avec l'extension .html
*
* @return null
*/

void to_HTML(const char* nom, char**res){
	char result[strlen(nom)] ;
	strcpy(result,nom);
	int j = strlen(nom)-1; 
	if(result[j-2]=='.' && result[j-1]=='m' && result[j] =='d'){
		char* res2= (char*)malloc(strlen(result)+3);
		strcpy(res2,result);
		res2[j-1] = 'h';
		res2[j] = 't';
		res2[j+1] = 'm'; 
		res2[j+2] = 'l';
		res2[j+3] = '\0';
		*res=res2;
	}else{
		char* res2= (char*)malloc(strlen(result)+6);
		strcpy(res2,result);
		res2[j+1] = '.';
		res2[j+2] = 'h';
		res2[j+3] = 't';
		res2[j+4] = 'm'; 
		res2[j+5] = 'l';
		res2[j+6] = '\0';
		*res=res2;
	}
}

/*
* Verifie la présence de l'extension .md
*
* @param file le nom du fichier a verifier.
*
* @return Retourne true si .md est present, false sinon
*/
bool is_MD(const char*file){
	int j = strlen(file)-1;  
	if(strlen(file) >= 4 && file[j-2]=='.' && file[j-1]=='m' && file[j] =='d')
		return true; 
	else
		return false;
}

/*
* Verifie la présence de l'extension .txt
*
* @param file le nom du fichier a verifier.
*
* @return Retourne true si .txt est present, false sinon
*/    
bool is_TXT(const char*file){
	int j = strlen(file)-1;  
	if(strlen(file) >= 5 && file[j-3]=='.' && file[j-2]=='t' && file[j-1] =='x' && file[j] =='t')
		return true; 
	else
		return false;
}

/*
* Fais la conversion vers html
*
* @param file le nom du fichier a convertir.
*
* @return Retourne 0 si succes, 1 sinon
*/
int noOption(const char* file){
	int status;
	int res = 0; 
	char* out ;
	to_HTML(file,&out);
	int pid = fork();
	if (pid == -1)
		perror("Echec du premier fork()");
	else if(pid == 0){
		execl( "/usr/bin/pandoc","pandoc","-o",out, file,NULL);
	} else {
		int ret = waitpid(pid, &status, 0);
		if (ret == -1)
			perror("echec de waitpid");
		if(res != 1)
			res = WEXITSTATUS(status); 
	}
return res;
}

/*
* Comportement du programme si -t est present
*
* @param file le nom du fichier .
*
* @return null
*/
bool optionT(const char* file){
	bool res = true; 
	struct stat source_stat;
	if(stat(file, &source_stat) < 0)
		exit(1);
	char* out;
	to_HTML(file,&out);
	struct stat dest_stat;
	if(stat(out, &dest_stat)<0){
		return true; 
	}
	if(source_stat.st_mtime > dest_stat.st_mtime){
	res = true;
	}else{
		res = false;      
		free(out);                        
	}
	return res; 
} 

/*
* Comportement du programme si -n est present
*
* @param file le nom du fichier .
*
* @return Retourne 0 si succes, 1 sinon
*/
int optionN(const char* file){
	int status;
	int res = 0; 
	struct stat source_stat;
	if(stat(file, &source_stat) < 0)
		return 1;
	int pid1 = fork();
	if (pid1 == -1)
		perror("Echec du premier fork()");
	else if(pid1 == 0){
		execl( "/usr/bin/echo","echo", file,NULL);
	}else {
		int ret = waitpid(pid1, &status, 0);
	if (ret == -1)
		perror("echec de waitpid");
	if(res != 1)
		res = WEXITSTATUS(status);
	}
	return res;
} 

/*
* Pour savoir si nous avons un repertoire ou non 
*
* @param dir le nom du fichier/parametre a verifier .
*
* @return Retourne true si repertoire, false sinon
*/
bool isDirectory(const char* dir) {
	bool res = false ; 
	struct stat dir_stat;
	if(stat(dir,&dir_stat) < 0)
		res =false ;
	switch (dir_stat.st_mode & S_IFMT) {
		case S_IFBLK:     break;
		case S_IFCHR:     break;
		case S_IFDIR:   res = true; break;
		case S_IFIFO:     break;
		case S_IFLNK:     break;
		case S_IFREG:     break;
		case S_IFSOCK:    break;
		default:    break;
    }

return res; 
}

/*
* Pour savoir si nous avons un lien symbolique ou non 
*
* @param dir le nom du fichier/parametre a verifier .
*
* @return Retourne true si lien symbolique, false sinon
*/
bool isSymLink(const char* file) {
        bool res = false ; 
        struct stat file_stat;
        if(lstat(file,&file_stat) < 0)
                res =false ;
        switch (file_stat.st_mode & S_IFMT) {
                case S_IFBLK:     		break;
                case S_IFCHR:     		break;
                case S_IFDIR:    		break;
                case S_IFIFO:     		break;
                case S_IFLNK:    		res = true; 	break;
                case S_IFREG:     		break;
                case S_IFSOCK:    		break;
                default:    				break;
    }

return res; 
}

/*
* Comportement du programme si -r est present
*
* @param name le nom du repertoire 
* @param t    Indique la présence de l'option t  
* @param n    Indique la présence de l'option n
* @param ret  Utiliser comme "return" pour savoir si les conversions sont 
*             bien faites ou non 
*
* @return null
*/
void listdir(const char *name,const bool t,const bool n,int* ret){
	DIR *dir;
	struct dirent *entry;
	if (!(dir = opendir(name))){
		*ret = 1;
		return;
	}
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type == DT_DIR) {
			char path[1024];
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
			snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
			listdir(path,t,n,ret);
		} else {
			if(isSymLink(entry->d_name)){
	                        struct stat file_stat;
	                        char* file2;
	                        ssize_t r;
	                        if(lstat(entry->d_name,&file_stat) == -1){
	                                *ret = 1;
	                                continue;
	                        }
	                        file2 = malloc(file_stat.st_size + 1);
	                        if(file2 == NULL){
	                                *ret = 1; 
	                                continue; 
	                        }
	                        r = readlink(entry->d_name, file2, file_stat.st_size + 1);
	                        if(r == -1){
	                                *ret = 1;
	                                continue;
	                        }
	                        if(r > file_stat.st_size){
	                                *ret = 1 ;
	                                continue;
	                        }
	                        file2[r] = '\0';
	                        if(t){
	                                if(optionT(file2)){
	                                        if(!n)
	                                                *ret = noOption(file2);
	                                        else
	                                                *ret = optionN(file2);
	                                }
        	                 }else{
        	                        if(!n)
        	                                *ret = noOption(file2);
        	                        else
        	                            	*ret = optionN(file2);
        	                 } 
        	        }else if(is_MD(entry->d_name)){
				char file[1024];
				snprintf(file,sizeof(file),"%s/%s",name,entry->d_name);	
				if(t==true){
					if(optionT(file) == true){
						if(n == true){
							*ret =  optionN(file);
						}else{
							*ret = noOption(file); 
						}
					} 	
				}else{
					if(n == true){
						*ret =  optionN(file);
					}else{
						*ret = noOption(file); 
					}
				}
			}
		}
	}

	    closedir(dir);
}


