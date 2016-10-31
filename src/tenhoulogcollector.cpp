#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
//#define WIN32_LEAN_AND_MEAN
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib/zlib.h"

#ifdef _WIN32
#include <conio.h>
#include <Windows.h>
#include <winhttp.h>
#else
#include <sys/types.h>
#include <dirent.h>
#endif

#define LOG_OUTPUT_FILENAME "logs.csv"
#define CONFIG_INI_PATH "\\AppData\\Local\\C-EGG\\tenhou\\130\\config.ini"
#define CSV_HEADER_V1 "number,log_id,player1,player2,player3,player4,first_oya," \
	"game_mode,points1,points2,points3,points4,score1,score2,score3,score4\n"
#define CSV_HEADER_V2 "number,log_id,player1,player2,player3,player4,first_oya," \
	"game_mode,points1,points2,points3,points4,score1,score2,score3,score4,rank1_raw,rank1,rating1,date\n"
#define CSV_HEADER_LEN 155
#define LOG_ID_MAXLEN 37
#define PLAYER_NAME_MAXLEN 256

#ifdef _WIN32
#define FLASH_STORAGE_PATH L"\\AppData\\Roaming\\Macromedia\\Flash Player\\#SharedObjects"
#define FLASH_STORAGE_PATH_CHROME L"\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Pepper Data\\Shockwave Flash\\WritableRoot\\#SharedObjects"
#elif defined(Macintosh) || defined(macintosh) || (defined(__APPLE__) && defined(__MACH__))
#define FLASH_STORAGE_PATH "/Library/Preferences/Macromedia/Flash Player/#SharedObjects/"
#define FLASH_STORAGE_PATH_CHROME "/Library/Application Support/Google/Chrome/Default/Pepper Data/Shockwave Flash/WritableRoot/#SharedObjects/"
#else//Linux/Unix
#define FLASH_STORAGE_PATH "/.macromedia/Flash_Player/#SharedObjects/"
#define FLASH_STORAGE_PATH_CHROME "/.config/google-chrome/Default/Pepper Data/Shockwave Flash/WritableRoot/#SharedObjects/"
#endif

struct loginfo_t {
	char log_id[LOG_ID_MAXLEN + 1];//null terminated
	char player_names[4][PLAYER_NAME_MAXLEN + 1];
	int first_oya;
	int game_mode;
	float points[4];
	int scores[4];
	//fields below are introduced in version 2 of the format
	int rank1;
	float rating1;
};

//returns format version if format is correct, 0 otherwise
//will move file position to the second line
int check_log_output_file_format(FILE *log_output_file)
{
	static char buf[CSV_HEADER_LEN + 1];

	if(fgets(buf, CSV_HEADER_LEN + 1, log_output_file) == NULL)
		return 0;

	if(strcmp(buf, CSV_HEADER_V1) == 0)
		return 1;//version 1
	if(strcmp(buf, CSV_HEADER_V2) == 0)
		return 2;//version 2

	return 0;//invalid header
}

//mode = 0 for read, 1 for write
//create the file if it does not exist
FILE *get_log_output_file(int mode)
{
	FILE *log_output_file;

	if(mode == 0)
	{
		log_output_file = fopen(LOG_OUTPUT_FILENAME, "r");
		if(log_output_file == NULL)
		{
			//file does not exist yet; create one
			log_output_file = fopen(LOG_OUTPUT_FILENAME, "w");
			if(log_output_file == NULL) return NULL;//cannot open file

			fputs(CSV_HEADER_V2, log_output_file);
			fclose(log_output_file);
			log_output_file = fopen(LOG_OUTPUT_FILENAME, "r");
			if(log_output_file == NULL) return NULL;//cannot open file
		}
	}
	else if(mode == 1)
		return fopen(LOG_OUTPUT_FILENAME, "w");

	return log_output_file;
}

//return 1 for success, 0 for failure
int csvline_to_loginfo(const char *buf, struct loginfo_t *loginfo)
{
	int i;
	const char *p, *q;
	static char buf2[512];

	p = strchr(buf, ',');
	if(p == NULL)
		return 0;

	p++;
	q = strchr(p, ',');
	if(q == NULL || q - p > LOG_ID_MAXLEN)
		return 0;

	memcpy(loginfo->log_id, p, q - p);
	loginfo->log_id[q - p] = 0;
	p = q + 1;
	for(i = 0; i < 4; i++)
	{
		q = strchr(p, ',');
		if(q == NULL || q - p > PLAYER_NAME_MAXLEN)
			return 0;

		memcpy(loginfo->player_names[i], p, q - p);
		loginfo->player_names[i][q - p] = 0;
		p = q + 1;
	}
	//try format version 2 first
	if(sscanf(p, "%d,%d,%f,%f,%f,%f,%d,%d,%d,%d,%d,%511s", &loginfo->first_oya, 
		&loginfo->game_mode, &loginfo->points[0], &loginfo->points[1], 
		&loginfo->points[2], &loginfo->points[3], &loginfo->scores[0],
		&loginfo->scores[1], &loginfo->scores[2], &loginfo->scores[3], 
		&loginfo->rank1, buf2) != 12)
	{
		//try format version 1 next
		if(sscanf(p, "%d,%d,%f,%f,%f,%f,%d,%d,%d,%d", &loginfo->first_oya, 
			&loginfo->game_mode, &loginfo->points[0], &loginfo->points[1], 
			&loginfo->points[2], &loginfo->points[3], &loginfo->scores[0],
			&loginfo->scores[1], &loginfo->scores[2], &loginfo->scores[3]) != 10)
			return 0;//row does not match either format version
		else
		{
			loginfo->rank1 = -1;
			loginfo->rating1 = 0.0f;
		}
	}
	else
	{
		p = strchr(buf2, ',');
		if(p == NULL) return 0;
		if(sscanf(p + 1, "%f", &loginfo->rating1) != 1)
			return 0;
	}

	return 1;
}

struct loginfo_t *mk_read_log_output(FILE *log_output_file, 
	int *num_entries_out, int *num_max_entries_out)
{
	static char buf[LOG_ID_MAXLEN + 4 * PLAYER_NAME_MAXLEN + 256];
	struct loginfo_t *loginfo;
	int loginfo_maxsize, num_entries;

	loginfo_maxsize = 256;
	loginfo = (struct loginfo_t *)malloc(sizeof(struct loginfo_t) * loginfo_maxsize);
	if(loginfo == NULL)
	{
		fprintf(stderr, "Error: not enough memory\n");
		return NULL;
	}

	if(check_log_output_file_format(log_output_file) == 0)
	{
		fprintf(stderr, "Error: existing %s is not in the correct format\n", LOG_OUTPUT_FILENAME);
		return NULL;
	}

	num_entries = 0;
	while(fgets(buf, LOG_ID_MAXLEN + 4 * PLAYER_NAME_MAXLEN + 256, 
		log_output_file) != NULL)
	{
		if(num_entries >= loginfo_maxsize)
		{
			loginfo_maxsize *= 2;
			loginfo = (struct loginfo_t *)realloc(loginfo, sizeof(struct loginfo_t) * 
				loginfo_maxsize);
			if(loginfo == NULL)
			{
				fprintf(stderr, "Error: not enough memory\n");
				return NULL;
			}
		}

		if(csvline_to_loginfo(buf, &loginfo[num_entries]) == 0)
		{
			fprintf(stderr, "Error: cannot parse entry %d in log output file\n", 
				num_entries + 1);
			free(loginfo);
			return NULL;
		}
		num_entries++;
	}

	*num_entries_out = num_entries;
	*num_max_entries_out = loginfo_maxsize;
	return loginfo;
}

int log_already_exists(struct loginfo_t *loginfo, int num_entries, 
	struct loginfo_t *loginfo_entry)
{
	int i;

	for(i = 0; i < num_entries; i++)
		if(strcmp(loginfo[i].log_id, loginfo_entry->log_id) == 0)
			return 1;

	return 0;
}

void collect_logs_from_windows_client(struct loginfo_t **loginfo, 
	int *num_entries, int *num_max_entries)
{
	FILE *in;
	const char *userprofile;
	char *path, *p, *q;
	int path_size, found, error, num_new_logs, n, ct;
	static char buf[LOG_ID_MAXLEN + 4 * PLAYER_NAME_MAXLEN + 256];
	struct loginfo_t loginfo_entry;

	if(*loginfo == NULL) return;

	userprofile = getenv("USERPROFILE");
	path_size = strlen(userprofile) + strlen(CONFIG_INI_PATH) + 1;
	path = (char *)malloc(sizeof(int) * path_size);
	sprintf(path, "%s%s", userprofile, CONFIG_INI_PATH);
	in = fopen(path, "r");
	free(path);
	if(in == NULL) return;//file does not exist

	//look for "[LOG]" line
	found = 0;
	while(fgets(buf, LOG_ID_MAXLEN + 4 * PLAYER_NAME_MAXLEN + 256, in) != NULL)
		if(strncmp(buf, "[LOG]", 5) == 0)
		{
			found = 1;
			break;
		}
	if(found)
	{
		error = 0;
		num_new_logs = 0;
		ct = 0;
		fgets(buf, LOG_ID_MAXLEN + 4 * PLAYER_NAME_MAXLEN + 256, in);//"N=#" line
		while(fgets(buf, LOG_ID_MAXLEN + 4 * PLAYER_NAME_MAXLEN + 256, in) != NULL)
		{
			buf[LOG_ID_MAXLEN + 4 * PLAYER_NAME_MAXLEN + 256 - 1] = 0;//make sure buf is null terminated
			if(sscanf(buf, "%d=", &n) != 1 || n != ct) break;//no more game log

			ct++;
			p = strstr(buf, "=file=");
			if(p == NULL) break;

			//log id
			p += 6;
			q = strchr(p, '&');
			if(q == NULL || q - p > LOG_ID_MAXLEN)
			{
				error = 2;
				break;
			}

			memcpy(loginfo_entry.log_id, p, q - p);
			loginfo_entry.log_id[q - p] = 0;
			if(log_already_exists(*loginfo, *num_entries, &loginfo_entry)) continue;

			if(*num_entries >= *num_max_entries)
			{
				*num_max_entries *= 2;
				*loginfo = (struct loginfo_t *)realloc(*loginfo, 
					sizeof(struct loginfo_t) * (*num_max_entries));
				if(*loginfo == NULL)
				{
					error = 1;
					break;
				}
			}
			strcpy((*loginfo)[*num_entries].log_id, loginfo_entry.log_id);

			//player1
			p = q + 1;
			if(strncmp(p, "un0=", 4) != 0)
			{
				error = 2;
				break;
			}
			p += 4;
			q = strchr(p, '&');
			if(q == NULL || q - p > PLAYER_NAME_MAXLEN)
			{
				error = 2;
				break;
			}
			memcpy((*loginfo)[*num_entries].player_names[0], p, q - p);
			(*loginfo)[*num_entries].player_names[0][q - p] = 0;
			p = q + 1;

			//player2
			if(strncmp(p, "un1=", 4) != 0)
			{
				error = 2;
				break;
			}
			p += 4;
			q = strchr(p, '&');
			if(q == NULL || q - p > PLAYER_NAME_MAXLEN)
			{
				error = 2;
				break;
			}
			memcpy((*loginfo)[*num_entries].player_names[1], p, q - p);
			(*loginfo)[*num_entries].player_names[1][q - p] = 0;
			p = q + 1;

			//player3
			if(strncmp(p, "un2=", 4) != 0)
			{
				error = 2;
				break;
			}
			p += 4;
			q = strchr(p, '&');
			if(q == NULL || q - p > PLAYER_NAME_MAXLEN)
			{
				error = 2;
				break;
			}
			memcpy((*loginfo)[*num_entries].player_names[2], p, q - p);
			(*loginfo)[*num_entries].player_names[2][q - p] = 0;
			p = q + 1;

			//player4
			if(strncmp(p, "un3=", 4) != 0)
			{
				error = 2;
				break;
			}
			p += 4;
			q = strchr(p, '&');
			if(q == NULL || q - p > PLAYER_NAME_MAXLEN)
			{
				error = 2;
				break;
			}
			memcpy((*loginfo)[*num_entries].player_names[3], p, q - p);
			(*loginfo)[*num_entries].player_names[3][q - p] = 0;
			p = q + 1;

			//first_oya
			if(strncmp(p, "oya=", 4) != 0 || p[4] < '0' || p[4] > '3')
			{
				error = 2;
				break;
			}
			p += 4;
			(*loginfo)[*num_entries].first_oya = (*p - '0') + 1;
			p += 2;
			
			//game mode
			if(strncmp(p, "type=", 5) != 0)
			{
				error = 2;
				break;
			}
			p += 5;
			if(sscanf(p, "%d", &(*loginfo)[*num_entries].game_mode) != 1)
			{
				error = 2;
				break;
			}

			//scores
			q = strchr(p, '&');
			if(q == NULL)
			{
				//no scores (game did not complete)
				(*loginfo)[*num_entries].scores[0] = 0;
				(*loginfo)[*num_entries].scores[1] = 0;
				(*loginfo)[*num_entries].scores[2] = 0;
				(*loginfo)[*num_entries].scores[3] = 0;
				(*loginfo)[*num_entries].points[0] = 0;
				(*loginfo)[*num_entries].points[1] = 0;
				(*loginfo)[*num_entries].points[2] = 0;
				(*loginfo)[*num_entries].points[3] = 0;
			}
			else
			{
				p = q + 1;
				if(strncmp(p, "sc=", 3) != 0)
				{
					error = 2;
					break;
				}
				p += 3;
				if(sscanf(p, "%d,%f,%d,%f,%d,%f,%d,%f", 
					&(*loginfo)[*num_entries].scores[0], &(*loginfo)[*num_entries].points[0], 
					&(*loginfo)[*num_entries].scores[1], &(*loginfo)[*num_entries].points[1], 
					&(*loginfo)[*num_entries].scores[2], &(*loginfo)[*num_entries].points[2], 
					&(*loginfo)[*num_entries].scores[3], &(*loginfo)[*num_entries].points[3]) 
					!= 8)
				{
					error = 2;
					break;
				}
				(*loginfo)[*num_entries].scores[0] *= 100;
				(*loginfo)[*num_entries].scores[1] *= 100;
				(*loginfo)[*num_entries].scores[2] *= 100;
				(*loginfo)[*num_entries].scores[3] *= 100;
			}
			(*loginfo)[*num_entries].rank1 = -1;
			(*loginfo)[*num_entries].rating1 = 0.0f;
			(*num_entries)++;
			num_new_logs++;
		}
		if(error == 0)
			printf("%d new game logs collected from Windows client\n", num_new_logs);
		else if(error == 1)
			fprintf(stderr, "Error: not enough memory\n");
		else if(error == 2)
		{
			*loginfo = NULL;
			fprintf(stderr, "Error: cannot parse Windows client's log links\n");
		}
	}
	fclose(in);
}

void wait_and_exit(int return_code)
{
#ifdef _WIN32
	printf("Press any key to exit...\n");
	_getch();
#else
	char buf[2];

	printf("Press enter to exit...\n");
	fgets(buf, 2, stdin);
#endif
	exit(return_code);
}

int loginfo_t_sf(const void *a, const void *b)
{
	struct loginfo_t *A = (struct loginfo_t *)a, *B = (struct loginfo_t *)b;

	return strcmp(A->log_id, B->log_id);
}

char *rank_to_str(int rank)
{
	switch(rank)
	{
	case 0:
		return "10k";
	case 1:
		return "9k";
	case 2:
		return "8k";
	case 3:
		return "7k";
	case 4:
		return "6k";
	case 5:
		return "5k";
	case 6:
		return "4k";
	case 7:
		return "3k";
	case 8:
		return "2k";
	case 9:
		return "1k";
	case 10:
		return "1d";
	case 11:
		return "2d";
	case 12:
		return "3d";
	case 13:
		return "4d";
	case 14:
		return "5d";
	case 15:
		return "6d";
	case 16:
		return "7d";
	case 17:
		return "8d";
	case 18:
		return "9d";
	case 19:
		return "10d";
	case 20:
		return "Tenhou";
	default:
		return "(unknown)";
	}
}

void write_loginfo_to_file(FILE *log_output_file, struct loginfo_t *loginfo, 
	int num_entries)
{
	int i;

	//sort by log id (based on timestamp)
	qsort(loginfo, num_entries, sizeof(loginfo[0]), loginfo_t_sf);
	fputs(CSV_HEADER_V2, log_output_file);
	for(i = 0; i < num_entries; i++)
		fprintf(log_output_file, "%d,%s,%s,%s,%s,%s,%d,%d,%.1f,%.1f,%.1f,%.1f,%d,%d,%d,%d,%d,%s,%f,%.8s\n", i + 1, loginfo[i].log_id, 
			loginfo[i].player_names[0], loginfo[i].player_names[1], loginfo[i].player_names[2], loginfo[i].player_names[3], 
			loginfo[i].first_oya, loginfo[i].game_mode, loginfo[i].points[0], loginfo[i].points[1], loginfo[i].points[2], 
			loginfo[i].points[3], loginfo[i].scores[0], loginfo[i].scores[1], loginfo[i].scores[2], loginfo[i].scores[3], 
			loginfo[i].rank1, rank_to_str(loginfo[i].rank1), loginfo[i].rating1, loginfo[i].log_id);
}

#ifdef _WIN32
//Note: flash storage location is different for Google Chrome browser
FILE *get_flash_storage_file(int *file_size_out, int is_chrome)
{
	FILE *in;
	HANDLE dir_handle, hFile;
	wchar_t *userprofile, *path;
	int path_size, rt;
	WIN32_FIND_DATA find_data;

	userprofile = _wgetenv(L"USERPROFILE");
	if(!is_chrome)
		path_size = wcslen(userprofile) + wcslen(FLASH_STORAGE_PATH) + 27 + 1;
	else
		path_size = wcslen(userprofile) + wcslen(FLASH_STORAGE_PATH_CHROME) + 27 + 1;
	path = (wchar_t *)malloc(sizeof(wchar_t) * path_size);
	if(!is_chrome)
		swprintf(path, path_size, L"%s%s\\*", userprofile, FLASH_STORAGE_PATH);
	else
		swprintf(path, path_size, L"%s%s\\*", userprofile, FLASH_STORAGE_PATH_CHROME);
	dir_handle = FindFirstFile(path, &find_data);
	if(dir_handle == INVALID_HANDLE_VALUE) return NULL;

	//get the name of the shared object directory within this directory
	do
	{
		if(find_data.cFileName[0] != L'.') break;
	}
	while(rt = FindNextFile(dir_handle, &find_data));
	FindClose(dir_handle);

	if(wcslen(find_data.cFileName) > 8)
	{
		fprintf(stderr, "Error: shared object directory name is longer than 8 characters\n");
		return NULL;
	}
	if(rt == 0) return NULL;//no directory found

	swprintf(path, path_size, L"%s%s\\%s\\mjv.jp\\mjinfo.sol", userprofile, 
		FLASH_STORAGE_PATH, find_data.cFileName);
	in = _wfopen(path, L"rb");
	
	hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	*file_size_out = GetFileSize(hFile, NULL);
	if(*file_size_out == INVALID_FILE_SIZE)
		*file_size_out = 0;
	free(path);
	CloseHandle(hFile);

	return in;
}
#else
FILE *get_flash_storage_file(int *file_size_out, int is_chrome)
{
	DIR *dir;
	struct dirent *entry;
	char *path, *userprofile;
	FILE *in;

	userprofile = getenv("HOME");
	if(userprofile == NULL) return NULL;
	if(!is_chrome)
	{
		path = (char *)malloc(sizeof(char) * (strlen(userprofile) + 
			strlen(FLASH_STORAGE_PATH) + 1));
		sprintf(path, "%s%s", userprofile, FLASH_STORAGE_PATH);
	}
	else
	{
		path = (char *)malloc(sizeof(char) * (strlen(userprofile) + 
			strlen(FLASH_STORAGE_PATH_CHROME) + 1));
		sprintf(path, "%s%s", userprofile, FLASH_STORAGE_PATH_CHROME);
	}
	dir = opendir(path);
	free(path);
	if(dir == NULL) return NULL;

	//get the name of the shared object directory within this directory
	while(1)
	{
		entry = readdir(dir);
		if(entry == NULL) return NULL;
		if(entry->d_type == DT_DIR && entry->d_name[0] != '.') break;
	}
	closedir(dir);

	if(!is_chrome)
	{
		path = (char *)malloc(sizeof(char) * (strlen(userprofile) + 
			strlen(FLASH_STORAGE_PATH) + strlen(entry->d_name) + 18 + 1));
		sprintf(path, "%s%s%s/mjv.jp/mjinfo.sol", userprofile, FLASH_STORAGE_PATH, 
			entry->d_name);
	}
	else
	{
		path = (char *)malloc(sizeof(char) * (strlen(userprofile) + 
			strlen(FLASH_STORAGE_PATH_CHROME) + strlen(entry->d_name) + 18 + 1));
		sprintf(path, "%s%s%s/mjv.jp/mjinfo.sol", userprofile, FLASH_STORAGE_PATH_CHROME, 
			entry->d_name);
	}

	in = fopen(path, "rb");
	if(in != NULL)
	{
		fseek(in, 0, SEEK_END);
		*file_size_out = ftell(in);
		if(*file_size_out == -1)
			*file_size_out = 0;
		fseek(in, 0, SEEK_SET);
	}
	return in;
}
#endif

#if !defined(_GNU_SOURCE)
void *memmem(const void *haystack, size_t haystacklen, const void *needle, 
	size_t needlelen)
{
	const char *p;

	p = (const char *)haystack;
	while(1)
	{
		if(haystacklen < needlelen) return NULL;
		if(memcmp(p, needle, needlelen) == 0) return (void *)p;
		p++;
		haystacklen--;
	}
}
#endif

int hex_char_to_int(char c)
{
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	return 0;
}

void unescape_log_id(char *dest, char *source)
{
	while(*source != 0)
	{
		if(*source == '%')
		{
			if(source[1] == 0 || source[2] == 0) break;
			*dest = hex_char_to_int(source[1]) * 16 + hex_char_to_int(source[2]);
			dest++;
			source += 3;
		}
		else
		{
			*dest = *source;
			dest++;
			source++;
		}
	}
	*dest = 0;
}

//Note: flash storage location is different for Google Chrome browser
void collect_logs_from_flash_client(struct loginfo_t **loginfo, 
	int *num_entries, int *num_max_entries, int is_chrome)
{
	FILE *in;
	char *p, *q, *buf;
	int file_size, bytes_read, error, num_new_logs;
	struct loginfo_t loginfo_entry;
	
	if(*loginfo == NULL) return;

	in = get_flash_storage_file(&file_size, is_chrome);
	if(in == NULL) return;//file does not exist

	buf = (char *)malloc(sizeof(char) * (file_size + 1024));
	if(buf == NULL)
	{
		*loginfo = NULL;
		fprintf(stderr, "Error: not enough memory\n");
		fclose(in);
		return;
	}

	bytes_read = fread(buf, sizeof(char), file_size + 1024, in);
	if(bytes_read < file_size || bytes_read == file_size + 1024)
	{
		*loginfo = NULL;
		if(bytes_read < file_size)
			fprintf(stderr, "Error: cannot read flash storage file\n");
		else
			fprintf(stderr, "Error: flash storage file changed between two reads\n");
		fclose(in);
		return;
	}

	buf[bytes_read] = 0;
	num_new_logs = 0;
	error = 0;
	p = (char *)memmem(buf, bytes_read, "file=", 5);
	while(p != NULL)
	{
		//log id
		p += 5;
		q = (char *)memchr(p, '&', buf + bytes_read - p);
		if(q == NULL || q - p > LOG_ID_MAXLEN)
		{
			error = 2;
			break;
		}
		memcpy(loginfo_entry.log_id, p, q - p);
		loginfo_entry.log_id[q - p] = 0;
		unescape_log_id(loginfo_entry.log_id, loginfo_entry.log_id);
		if(log_already_exists(*loginfo, *num_entries, &loginfo_entry))
		{
			p = (char *)memmem(p, buf + bytes_read - p, "file=", 5);
			continue;
		}

		if(*num_entries >= *num_max_entries)
		{
			*num_max_entries *= 2;
			*loginfo = (struct loginfo_t *)realloc(*loginfo, 
				sizeof(struct loginfo_t) * (*num_max_entries));
			if(*loginfo == NULL)
			{
				error = 1;
				break;
			}
		}
		strcpy((*loginfo)[*num_entries].log_id, loginfo_entry.log_id);
		p = q + 1;

		//player1
		if(buf + bytes_read - p < 4 || strncmp(p, "un0=", 4) != 0)
		{
			error = 2;
			break;
		}
		p += 4;
		q = (char *)memchr(p, '&', buf + bytes_read - p);
		if(q == NULL || q - p > PLAYER_NAME_MAXLEN)
		{
			error = 2;
			break;
		}
		memcpy((*loginfo)[*num_entries].player_names[0], p, q - p);
		(*loginfo)[*num_entries].player_names[0][q - p] = 0;
		p = q + 1;

		//player2
		if(buf + bytes_read - p < 4 || strncmp(p, "un1=", 4) != 0)
		{
			error = 2;
			break;
		}
		p += 4;
		q = (char *)memchr(p, '&', buf + bytes_read - p);
		if(q == NULL || q - p > PLAYER_NAME_MAXLEN)
		{
			error = 2;
			break;
		}
		memcpy((*loginfo)[*num_entries].player_names[1], p, q - p);
		(*loginfo)[*num_entries].player_names[1][q - p] = 0;
		p = q + 1;

		//player3
		if(buf + bytes_read - p < 4 || strncmp(p, "un2=", 4) != 0)
		{
			error = 2;
			break;
		}
		p += 4;
		q = (char *)memchr(p, '&', buf + bytes_read - p);
		if(q == NULL || q - p > PLAYER_NAME_MAXLEN)
		{
			error = 2;
			break;
		}
		memcpy((*loginfo)[*num_entries].player_names[2], p, q - p);
		(*loginfo)[*num_entries].player_names[2][q - p] = 0;
		p = q + 1;

		//player4
		if(buf + bytes_read - p < 4 || strncmp(p, "un3=", 4) != 0)
		{
			error = 2;
			break;
		}
		p += 4;
		q = (char *)memchr(p, '&', buf + bytes_read - p);
		if(q == NULL || q - p > PLAYER_NAME_MAXLEN)
		{
			error = 2;
			break;
		}
		memcpy((*loginfo)[*num_entries].player_names[3], p, q - p);
		(*loginfo)[*num_entries].player_names[3][q - p] = 0;

		//first oya
		p = q + 1;
		if(buf + bytes_read - p < 5 || strncmp(p, "oya=", 4) != 0 || p[4] < '0' || p[4] > '3')
		{
			error = 2;
			break;
		}
		p += 4;
		(*loginfo)[*num_entries].first_oya = (*p - '0') + 1;
		p += 2;

		//game mode
		if(buf + bytes_read - p < 6 || strncmp(p, "type=", 5) != 0)
		{
			error = 2;
			break;
		}
		p += 5;
		if(sscanf(p, "%d", &(*loginfo)[*num_entries].game_mode) != 1)
		{
			error = 2;
			break;
		}

		//scores
		q = (char *)memchr(p, '&', buf + bytes_read - p);
		if(q == NULL)
		{
			//no scores (game did not complete)
			(*loginfo)[*num_entries].scores[0] = 0;
			(*loginfo)[*num_entries].scores[1] = 0;
			(*loginfo)[*num_entries].scores[2] = 0;
			(*loginfo)[*num_entries].scores[3] = 0;
			(*loginfo)[*num_entries].points[0] = 0;
			(*loginfo)[*num_entries].points[1] = 0;
			(*loginfo)[*num_entries].points[2] = 0;
			(*loginfo)[*num_entries].points[3] = 0;
		}
		else
		{
			p = q + 1;
			if(buf + bytes_read - p < 4 || strncmp(p, "sc=", 3) != 0)
			{
				error = 2;
				break;
			}
			p += 3;
			if(sscanf(p, "%d,%f,%d,%f,%d,%f,%d,%f", 
				&(*loginfo)[*num_entries].scores[0], &(*loginfo)[*num_entries].points[0], 
				&(*loginfo)[*num_entries].scores[1], &(*loginfo)[*num_entries].points[1], 
				&(*loginfo)[*num_entries].scores[2], &(*loginfo)[*num_entries].points[2], 
				&(*loginfo)[*num_entries].scores[3], &(*loginfo)[*num_entries].points[3]) 
				!= 8)
			{
				error = 2;
				break;
			}
			(*loginfo)[*num_entries].scores[0] *= 100;
			(*loginfo)[*num_entries].scores[1] *= 100;
			(*loginfo)[*num_entries].scores[2] *= 100;
			(*loginfo)[*num_entries].scores[3] *= 100;
		}
		(*loginfo)[*num_entries].rank1 = -1;
		(*loginfo)[*num_entries].rating1 = 0.0f;
		(*num_entries)++;
		num_new_logs++;
		p = (char *)memmem(p, buf + bytes_read - p, "file=", 5);
	}
	if(error == 0)
		printf("%d new logs collected from Flash client%s\n", num_new_logs, 
			is_chrome ? " on Google Chrome" : "");
	else if(error == 1)
		fprintf(stderr, "Error: not enough memory\n");
	if(error == 2)
	{
		*loginfo = NULL;
		fprintf(stderr, "Error: cannot parse flash storage file\n");
	}

	free(buf);
	fclose(in);
}

//returns default directory if argument not specified
char *get_log_directory(int argc, char *argv[])
{
	int i;

	for(i = 1; i < argc - 1; i++)
		if(strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--directory") == 0) return argv[i + 1];
	return "mjlog";
}

//returns 1 if flag is set, 0 otherwise
int get_nowait_arg_flag(int argc, char *argv[])
{
	int i;

	for(i = 1; i < argc; i++)
		if(strcmp(argv[i], "/nowait") == 0 || strcmp(argv[i], "--nowait") == 0) return 1;
	return 0;
}

//returns 1 if the directory already exists or is created successfully, 0 otherwise
int create_log_directory(const char *log_directory)
{
	static WCHAR path[256];
	int rt;

	mbstowcs(path, log_directory, ARRAYSIZE(path));
	path[ARRAYSIZE(path) - 1] = L'\x0';
	rt = CreateDirectory(path, NULL);
	if(rt != 0 || GetLastError() == ERROR_ALREADY_EXISTS)
		return 1;
	return 0;
}

void print_error_and_exit(char *s,...)
{
	va_list arguments;

	va_start(arguments,s);
	vprintf(s,arguments);
	va_end(arguments);
	system("pause");
	exit(1);
}

void download_url_init(HINTERNET *hSession, HINTERNET *hConnect, WCHAR *server, int port)
{
	*hSession = WinHttpOpen(NULL, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if(*hSession == NULL) print_error_and_exit("Failed initializing WinHttp\n");

	*hConnect = WinHttpConnect(*hSession, server, port, 0);
	if(*hConnect == NULL) print_error_and_exit("Failed initializing WinHttp\n");
}

void download_url_finalize(HINTERNET hSession, HINTERNET hConnect)
{
	if(hConnect) WinHttpCloseHandle(hConnect);
	if(hSession) WinHttpCloseHandle(hSession);
}

//returns NULL or calls my_error if failed
char *mk_download_url_persistent(HINTERNET hSession, HINTERNET hConnect, WCHAR *path, int *num_bytes_out)
{
	HINTERNET hRequest;
	DWORD num_bytes_read, t, header_buf_len;
	char *buf, *tight_buf;
	unsigned char *uncompressed_buf;
	const TCHAR *headers;
	TCHAR *header_buf;
	size_t buf_size;

	hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL, 
		WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
	if(hRequest == NULL) print_error_and_exit("WinHttpOpenRequest failed\n");

	headers = TEXT("Accept-Encoding: gzip, deflate");
	if(!WinHttpSendRequest(hRequest, headers, -1L, WINHTTP_NO_REQUEST_DATA, 0, 0, NULL))
	//if(!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, NULL))
	{
		if(hRequest) WinHttpCloseHandle(hRequest);
		return NULL;
	}

	if(!WinHttpReceiveResponse(hRequest, NULL))
	{
		if(hRequest) WinHttpCloseHandle(hRequest);
		return NULL;
	}

	num_bytes_read = 0;
	buf_size = 0;
	while(1)
	{
		if(!WinHttpQueryDataAvailable(hRequest, &t))
		{
			if(buf_size > 0)
				free(buf);
			if(hRequest) WinHttpCloseHandle(hRequest);
			return NULL;
		}

		if(t <= 0) break;//done!

		if(num_bytes_read + t > buf_size)
		{
			if(buf_size == 0)
			{
				buf_size = 32768;//initial size
				while(buf_size < t)
				{
					if(buf_size >= 64 * 1048576)
						print_error_and_exit("Game log is too big (larger than 64 MB)\n");
					buf_size <<= 1;
				}
				buf = (char *)malloc(sizeof(char) * buf_size);
			}
			else
			{
				while(buf_size < num_bytes_read + t)
				{
					if(buf_size >= 64 * 1048576)
						print_error_and_exit("Game log is too big (larger than 64 MB)\n");
					buf_size <<= 1;
				}
				buf = (char *)realloc(buf, sizeof(char) * buf_size);
			}
		}
		
		if(!WinHttpReadData(hRequest, buf + num_bytes_read, (int)(buf_size - num_bytes_read), &t))
			print_error_and_exit("WinHttpReadData failed\n");
		if(t == 0)
			print_error_and_exit("WinHttpReadData returned 0 bytes\n");
		num_bytes_read += t;
	}
	//check if buf needs to be decompressed
	WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_ENCODING, WINHTTP_HEADER_NAME_BY_INDEX, 
		NULL, &header_buf_len, WINHTTP_NO_HEADER_INDEX);
	header_buf = (TCHAR *)malloc(sizeof(char) * header_buf_len);
	WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_ENCODING, WINHTTP_HEADER_NAME_BY_INDEX, 
		header_buf, &header_buf_len, WINHTTP_NO_HEADER_INDEX);
	if(wcscmp(header_buf, TEXT("gzip")) == 0)
	{
		unsigned char *uncompressed_buf;
		unsigned long uncompressed_buf_size = num_bytes_read * 8;
		//FILE *out = fopen("D:\\Downloads\\del.gz", "wb");

		//fwrite(buf, 1, num_bytes_read, out);
		//fclose(out);
		uncompressed_buf = (unsigned char *)malloc(sizeof(char) * uncompressed_buf_size);
		if(gzuncompress(uncompressed_buf, &uncompressed_buf_size, (unsigned char *)buf, num_bytes_read) != Z_OK)
			print_error_and_exit("Cannot decompress game log\n");
		tight_buf = (char *)malloc(sizeof(char) * (uncompressed_buf_size + 1));
		memcpy(tight_buf, uncompressed_buf, uncompressed_buf_size);
		tight_buf[uncompressed_buf_size] = 0;//null terminate it
		free(uncompressed_buf);
		num_bytes_read = uncompressed_buf_size;
	}
	else//plain text
	{
		tight_buf = (char *)malloc(sizeof(char) * (num_bytes_read + 1));
		memcpy(tight_buf, buf, num_bytes_read);
		tight_buf[num_bytes_read] = 0;//null terminate it
	}
	free(header_buf);
	*num_bytes_out = num_bytes_read;
	free(buf);

	if(hRequest) WinHttpCloseHandle(hRequest);

	return tight_buf;
}

//step = 0 for initialization, 1 for downloading, 2 for finalization
char *mk_download_mjlog(const char *log_fn, int step)
{
	char *buf;
	WCHAR path[1024];
	int num_bytes, tries;
	size_t rt;
	static HINTERNET hSession = NULL, hConnect = NULL;

	if(step == 0)
	{
		download_url_init(&hSession, &hConnect, L"tenhou.net", INTERNET_DEFAULT_HTTP_PORT);
		return NULL;
	}
	if(step == 2)
	{
		download_url_finalize(hSession, hConnect);
		return NULL;
	}

	_snwprintf_s(path, 1024, _TRUNCATE, L"/0/log/?");
	if(mbstowcs_s(&rt, path + 8, 1024 - 8, log_fn, 1024 - 8) != 0) print_error_and_exit("mbstowcs_s failed");
	printf("Downloading http://tenhou.net/0/log/?%s\n", log_fn);

	for(tries = 0; tries < 4; tries++)
	{
		buf = mk_download_url_persistent(hSession, hConnect, path, &num_bytes);
		if(buf != NULL) break;//success
		if(tries == 0)
			printf("Failed to download. Retrying.\n");
		else
			printf("Failed to download. Retrying after %d seconds.\n", tries);
		Sleep(tries * 1000);
	}
	if(buf == NULL)
		print_error_and_exit("Failed to download game log.\n");
	if(num_bytes < 19 || strncmp(buf, "<mjloggm ver=\"2.3\">", 19) != 0)
		print_error_and_exit("Invalid data received (%d bytes). First 20 bytes: \"%.20s\"\n", num_bytes, buf);

	return buf;
}

//return 0 if successful, -1 if failed
int extract_dan_and_rating_from_game_log(const char *mjlog_buf, struct loginfo_t *loginfo)
{
	const char *p;
	int seat_index, ranks[4];
	float ratings[4];

	seat_index = (4 - (loginfo->first_oya - 1)) % 4;
	p = strstr(mjlog_buf, "dan=\"");
	if(p == NULL) return -1;
	if(sscanf(p + 5, "%d,%d,%d,%d", &ranks[0], &ranks[1], &ranks[2], &ranks[3]) != 4) return -1;
	loginfo->rank1 = ranks[seat_index];
	p = strstr(mjlog_buf, "rate=\"");
	if(p == NULL) return -1;
	if(sscanf(p + 6, "%f,%f,%f,%f", &ratings[0], &ratings[1], &ratings[2], &ratings[3]) != 4) return -1;
	loginfo->rating1 = ratings[seat_index];

	return 0;
}

void write_mjlog_to_file(const char *mjlog_buf, const char *log_directory, 
	const char *log_id, int seat_index)
{
	static char filename[1024];
	FILE *out;
	int len;

	if(log_directory[strlen(log_directory) - 1] == '\\')
		_snprintf_s(filename, ARRAYSIZE(filename), _TRUNCATE, "%s%s&tw=%d.mjlog", log_directory, log_id, seat_index);
	else
		_snprintf_s(filename, ARRAYSIZE(filename), _TRUNCATE, "%s\\%s&tw=%d.mjlog", log_directory, log_id, seat_index);
	out = fopen(filename, "w");
	if(out == NULL) print_error_and_exit("Cannot write to %s\n", filename);

	len = strlen(mjlog_buf);
	if(fwrite(mjlog_buf, sizeof(char), len, out) != len)
		fprintf(stderr, "Error writing to %s\n", filename);
	fclose(out);
}

void get_loginfo_rank_rating(struct loginfo_t *loginfo, int num_entries, const char *log_directory)
{
	int i, winhttp_inited, num_game_logs_downloaded;
	char *mjlog_buf;

	mjlog_buf = NULL;
	winhttp_inited = 0;
	num_game_logs_downloaded = 0;
	for(i = 0; i < num_entries; i++)
	{
		if(loginfo[i].rank1 == -1 || loginfo[i].rating1 == 0.0)
		{
			if(winhttp_inited == 0)
			{
				mk_download_mjlog(NULL, 0);//winhttp initialization
				winhttp_inited = 1;
			}
			mjlog_buf = mk_download_mjlog(loginfo[i].log_id, 1);
			if(mjlog_buf == NULL)
			{
				mk_download_mjlog(NULL, 2);//winhttp finalization
				print_error_and_exit("Error downloading game log\n");
			}
			if(extract_dan_and_rating_from_game_log(mjlog_buf, &loginfo[i]) != 0)
			{
				mk_download_mjlog(NULL, 2);//winhttp finalization
				print_error_and_exit("Invalid log format\n");
			}
			num_game_logs_downloaded++;
			write_mjlog_to_file(mjlog_buf, log_directory, loginfo[i].log_id, (4 - (loginfo[i].first_oya - 1)) % 4);
			free(mjlog_buf);
		}
	}
	if(winhttp_inited == 1) mk_download_mjlog(NULL, 2);//winhttp finalization
	printf("%d game logs downloaded and saved in directory %s\n", num_game_logs_downloaded, log_directory);
}

int main(int argc, char *argv[])
{
	FILE *log_output_file;
	struct loginfo_t *loginfo;
	int num_entries, loginfo_maxsize, nowait_flag;
	const char *log_directory;

	nowait_flag = get_nowait_arg_flag(argc, argv);
	log_directory = get_log_directory(argc, argv);
	create_log_directory(log_directory);
	log_output_file = get_log_output_file(0);
	if(log_output_file == NULL)
	{
		fprintf(stderr, "Error: cannot open %s. Is the file currently opened by another program?\n", LOG_OUTPUT_FILENAME);
		wait_and_exit(1);
	}
	loginfo = mk_read_log_output(log_output_file, &num_entries, &loginfo_maxsize);
	fclose(log_output_file);

#ifdef _WIN32
	collect_logs_from_windows_client(&loginfo, &num_entries, &loginfo_maxsize);
#endif
	collect_logs_from_flash_client(&loginfo, &num_entries, &loginfo_maxsize, 0);
	collect_logs_from_flash_client(&loginfo, &num_entries, &loginfo_maxsize, 1);
	if(loginfo == NULL)
		wait_and_exit(1);
	else
	{
		get_loginfo_rank_rating(loginfo, num_entries, log_directory);
		log_output_file = get_log_output_file(1);
		if(log_output_file == NULL)
		{
			fprintf(stderr, "Error: cannot open %s. Is the file currently opened by another program?\n", LOG_OUTPUT_FILENAME);
			wait_and_exit(1);
		}
		write_loginfo_to_file(log_output_file, loginfo, num_entries);
		fclose(log_output_file);
		printf("Logs saved to %s\n", LOG_OUTPUT_FILENAME);
	}
	if(!nowait_flag) wait_and_exit(0);
	
	return 0;
}
