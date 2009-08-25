/** Uncompress input files using pipes.
 * Hook the standard file opening functions, open, fopen and fopen64.
 * If the extension of the file being opened indicates the file is
 * compressed (.gz or .bz2), open a pipe to a program that
 * decompresses that file (zcat or bzcat), and return a handle to the
 * open pipe.
 * @author Shaun Jackman <sjackman@bcgsc.ca>
 */

#include <cassert>
#include <cstdio> // for perror
#include <cstdlib>
#include <dlfcn.h>
#include <string>
#include <unistd.h>

using namespace std;

/** Tests whether this string ends with the specified suffix. */
static bool endsWith(const string& s, const string& suffix)
{
	ssize_t i = s.length() - suffix.length();
	return i < 0 ? false : s.substr(i) == suffix;
}

static const char* zcatExec(const string& path)
{
	return
		endsWith(path, ".gz") ? "zcat" :
		endsWith(path, ".bz2") ? "bzcat" :
		NULL;
}

static string zcatCommand(const string& path)
{
	const char *zcat = zcatExec(path);
	return zcat == NULL ? "" : string(zcat) + ' ' + path;
}

extern "C" {

typedef FILE* (*fopen_t)(const char *path, const char *mode);

/** If the specified file is compressed, return a pipe that
 * uncompresses it.
 */
FILE *fopen(const char *path, const char *mode)
{
	static fopen_t real_fopen;
	if (real_fopen == NULL)
		real_fopen = (fopen_t)dlsym(RTLD_NEXT, "fopen");
	assert(real_fopen != NULL);

	string command = zcatCommand(string(path));
	return command.empty() ? real_fopen(path, mode)
		: popen(command.c_str(), mode);
}

/** If the specified file is compressed, return a pipe that
 * uncompresses it.
 */
FILE *fopen64(const char *path, const char *mode)
{
	static fopen_t real_fopen64;
	if (real_fopen64 == NULL)
		real_fopen64 = (fopen_t)dlsym(RTLD_NEXT, "fopen64");
	assert(real_fopen64 != NULL);

	string command = zcatCommand(string(path));
	return command.empty() ? real_fopen64(path, mode)
		: popen(command.c_str(), mode);
}

typedef int (*open_t)(const char *path, int flags, mode_t mode);

/** If the specified file is compressed, return a pipe that
 * uncompresses it.
 */
int open(const char *path, int flags, mode_t mode)
{
	static open_t real_open;
	if (real_open == NULL)
		real_open = (open_t)dlsym(RTLD_NEXT, "open");
	assert(real_open != NULL);

	const char *zcat = zcatExec(path);
	if (zcat == NULL)
		return real_open(path, flags, mode);

	int fd[2];
	if (pipe(fd) == -1)
		return -1;

	pid_t pid = fork();
	if (pid == -1)
		return -1;

	if (pid == 0) {
		close(fd[1]);
		return fd[0];
	} else {
		close(fd[0]);
		dup2(fd[1], STDOUT_FILENO);
		execlp(zcat, zcat, path, NULL);
		perror(zcat);
		exit(EXIT_FAILURE);
	}
}

} // extern "C"
