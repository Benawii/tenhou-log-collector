#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#include <Windows.h>
#else
#include <sys/types.h>
#include <dirent.h>
#endif

#define LOG_OUTPUT_FILENAME "logs.csv"
#define CONFIG_INI_PATH "\\AppData\\Local\\C-EGG\\tenhou\\130\\config.ini"
#define CSV_HEADER "number,log_id,player1,player2,player3,player4,first_oya," \
	"game_mode,points1,points2,points3,points4,score1,score2,score3,score4\n"
#define CSV_HEADER_LEN 127
#define LOG_ID_MAXLEN 37
#define PLAYER_NAME_MAXLEN 256

#ifdef _WIN32
#define FLASH_STORAGE_PATH L"\\AppData\\Roaming\\Macromedia\\Flash Player\\#SharedObjects"
#define FLASH_STORAGE_PATH_CHROME L"\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Pepper Data\\Shockwave Flash\\WritableRoot\\#SharedObjects"
#elif defined(Macintosh) || defined(macintosh) || (defined(__APPLE__) && defined(__MACH__))
#define FLASH_STORAGE_PATH "/Library/Preferences/Macromedia/Flash Player/#SharedObjects/"
#define FLASH_STORAGE_PATH_CHROME "/Library/Application Support/Google/Chrome/Default/Pepper Data/Shockwave Flash/WritableRoot/#SharedObjects/"
#else
#define FLASH_STORAGE_PATH "/.macromedia/Flash_Player/#SharedObjects/"
#define FLASH_STORAGE_PATH_CHROME "/.config/google-chrome/Default/Pepper Data/Shockwave Flash/WritableRoot/#SharedObjects/"
#endif

int log_exists = 0;

struct loginfo_t {
	char log_id[LOG_ID_MAXLEN + 1];
	char player_names[4][PLAYER_NAME_MAXLEN + 1];
	int first_oya;
	int game_mode;
	float points[4];
	int scores[4];
};

//returns 1 if format is correct, 0 otherwise
//will move file position to the second line
int check_log_output_file_format(FILE *log_output_file)
{
	static char buf[CSV_HEADER_LEN + 1];

	if(fgets(buf, CSV_HEADER_LEN + 1, log_output_file) == NULL)
		return 0;

	if(strcmp(buf, CSV_HEADER) != 0)
		return 0;

	return 1;
}

//create the file if it does not exist
FILE *get_log_output_file()
{
	FILE *log_output_file;

	log_output_file = fopen(LOG_OUTPUT_FILENAME, "r+");
	if(log_output_file == NULL)
	{
		log_output_file = fopen(LOG_OUTPUT_FILENAME, "w");
		if(log_output_file == NULL) return NULL;//cannot open file

		fputs(CSV_HEADER, log_output_file);
		fclose(log_output_file);
		log_output_file = fopen(LOG_OUTPUT_FILENAME, "r+");
		log_exists = 0;
	}
	else log_exists = 1;

	check_log_output_file_format(log_output_file);

	return log_output_file;
}

//return 1 for success, 0 for failure
int csvline_to_loginfo(const char *buf, struct loginfo_t *loginfo)
{
	int i;
	const char *p, *q;

	p = strchr(buf, ',');
	if(p == NULL) return 0;

	p++;
	q = strchr(p, ',');
	if(q == NULL || q - p > LOG_ID_MAXLEN) return 0;

	memcpy(loginfo->log_id, p, q - p);
	loginfo->log_id[q - p] = 0;
	p = q + 1;
	for(i = 0; i < 4; i++)
	{
		q = strchr(p, ',');
		if(q == NULL || q - p > PLAYER_NAME_MAXLEN) return 0;

		memcpy(loginfo->player_names[i], p, q - p);
		loginfo->player_names[i][q - p] = 0;
		p = q + 1;
	}
	if(sscanf(p, "%d,%d,%f,%f,%f,%f,%d,%d,%d,%d", &loginfo->first_oya, 
		&loginfo->game_mode, &loginfo->points[0], &loginfo->points[1], 
		&loginfo->points[2], &loginfo->points[3], &loginfo->scores[0],
		&loginfo->scores[1], &loginfo->scores[2], &loginfo->scores[3]) != 10)
		return 0;

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
	int path_size, found, error, num_new_logs;
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
		fgets(buf, LOG_ID_MAXLEN + 4 * PLAYER_NAME_MAXLEN + 256, in);
		while(fgets(buf, LOG_ID_MAXLEN + 4 * PLAYER_NAME_MAXLEN + 256, in) != NULL)
		{
			if(buf[0] == '[')
				break;
			p = strstr(buf, "=file=");
			if(p == NULL)
			{
				error = 2;
				break;
			}

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
			if(strncmp(p, "oya=", 4) != 0 || p[4] < '0' || p[4] > '3')
			{
				error = 2;
				break;
			}
			p += 4;
			(*loginfo)[*num_entries].first_oya = (*p - '0') + 1;
			p += 2;
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
			(*num_entries)++;
			num_new_logs++;
		}
		if(error == 0)
			printf("%d new logs collected from windows client\n", num_new_logs);
		else if(error == 1)
			fprintf(stderr, "Error: not enough memory\n");
		else if(error == 2)
		{
			*loginfo = NULL;
			fprintf(stderr, "Error: cannot parse log links\n");
		}
	}
	fclose(in);
}

void wait_and_exit()
{
#ifdef _WIN32
	printf("Press any key to exit...\n");
	_getch();
#else
	char buf[2];

	printf("Press enter to exit...\n");
	fgets(buf, 2, stdin);
#endif
	exit(0);
}

int loginfo_t_sf(const void *a, const void *b)
{
	struct loginfo_t *A = (struct loginfo_t *)a, *B = (struct loginfo_t *)b;

	return strcmp(A->log_id, B->log_id);
}

void write_loginfo_to_file(FILE *log_output_file, struct loginfo_t *loginfo, 
	int num_entries)
{
	int i;

	qsort(loginfo, num_entries, sizeof(loginfo[0]), loginfo_t_sf);
	fseek(log_output_file, 0, SEEK_SET);
	fputs(CSV_HEADER, log_output_file);
	for(i = 0; i < num_entries; i++)
		fprintf(log_output_file, "%d,%s,%s,%s,%s,%s,%d,%d,%.1f,%.1f,%.1f,%.1f,%d,%d,%d,%d\n", i + 1, loginfo[i].log_id, 
			loginfo[i].player_names[0], loginfo[i].player_names[1], loginfo[i].player_names[2], loginfo[i].player_names[3], 
			loginfo[i].first_oya, loginfo[i].game_mode, loginfo[i].points[0], loginfo[i].points[1], loginfo[i].points[2], 
			loginfo[i].points[3], loginfo[i].scores[0], loginfo[i].scores[1], loginfo[i].scores[2], loginfo[i].scores[3]);
}

#ifdef _WIN32
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
		swprintf(path, L"%s%s\\*", userprofile, FLASH_STORAGE_PATH);
	else
		swprintf(path, L"%s%s\\*", userprofile, FLASH_STORAGE_PATH_CHROME);
	dir_handle = FindFirstFile(path, &find_data);
	if(dir_handle == INVALID_HANDLE_VALUE) return NULL;

	rt = 1;
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

	swprintf(path, L"%s%s\\%s\\mjv.jp\\mjinfo.sol", userprofile, 
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

void collect_logs_from_flash_client(struct loginfo_t **loginfo, 
	int *num_entries, int *num_max_entries, int is_chrome)
{
	FILE *in;
	char *p, *q, *buf;
	int file_size, bytes_read, error, num_new_logs;
	struct loginfo_t loginfo_entry;
	
	in = get_flash_storage_file(&file_size, is_chrome);
	if(in == NULL)
	{
		printf("%slash log not found\n", is_chrome ? "Chrome's f" : "F");
		return;//file does not exist
	}

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
			fprintf(stderr, "Error: file changed too much between two reads\n");
		fclose(in);
		return;
	}

	buf[bytes_read] = 0;
	num_new_logs = 0;
	error = 0;
	p = (char *)memmem(buf, bytes_read, "file=", 5);
	while(p != NULL)
	{
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

		p = q + 1;
		if(buf + bytes_read - p < 5 || strncmp(p, "oya=", 4) != 0 || p[4] < '0' || p[4] > '3')
		{
			error = 2;
			break;
		}
		p += 4;
		(*loginfo)[*num_entries].first_oya = (*p - '0') + 1;
		p += 2;
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
		(*num_entries)++;
		num_new_logs++;
		p = (char *)memmem(p, buf + bytes_read - p, "file=", 5);
	}
	if(error == 0)
		printf("%d new logs collected from flash client\n", num_new_logs);
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

//returns 1 if flag is set, 0 otherwise
int get_nowait_flag(int argc, char *argv[])
{
	int i;

	for(i = 1; i < argc; i++)
		if(strcmp(argv[i], "/nowait") == 0) return 1;
	return 0;
}

int main(int argc, char *argv[])
{
	FILE *log_output_file;
	struct loginfo_t *loginfo;
	int num_entries, loginfo_maxsize, nowait_flag;

	nowait_flag = get_nowait_flag(argc, argv);
	log_output_file = get_log_output_file();
	if(log_output_file == NULL)
	{
		fprintf(stderr, "Error: cannot open logs.csv. Is the file currently opened by another program?\n");
		wait_and_exit();
	}
	loginfo = mk_read_log_output(log_output_file, &num_entries, &loginfo_maxsize);

#ifdef _WIN32
	collect_logs_from_windows_client(&loginfo, &num_entries, &loginfo_maxsize);
#endif
	collect_logs_from_flash_client(&loginfo, &num_entries, &loginfo_maxsize, 0);
	collect_logs_from_flash_client(&loginfo, &num_entries, &loginfo_maxsize, 1);
	if(loginfo == NULL)
	{
		fclose(log_output_file);
		wait_and_exit();
	}
	else
	{
		write_loginfo_to_file(log_output_file, loginfo, num_entries);
		fclose(log_output_file);
	}
	if(!nowait_flag) wait_and_exit();
	
	return 0;
}
