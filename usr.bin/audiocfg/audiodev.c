/* $NetBSD: audiodev.c,v 1.1.1.1 2010/08/30 02:19:47 mrg Exp $ */

/*
 * Copyright (c) 2010 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/drvctlio.h>

#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audiodev.h"
#include "drvctl.h"

static TAILQ_HEAD(audiodevhead, audiodev) audiodevlist =
    TAILQ_HEAD_INITIALIZER(audiodevlist);

static int
audiodev_getinfo(struct audiodev *adev)
{
	struct stat st;

	if (stat(adev->path, &st) == -1)
		return -1;
	adev->dev = st.st_rdev;

	if (stat(_PATH_AUDIO, &st) != -1 && st.st_rdev == adev->dev)
		adev->defaultdev = true;

	adev->fd = open(adev->path, O_RDWR);
	if (adev->fd == -1)
		return -1;
	if (ioctl(adev->fd, AUDIO_GETDEV, &adev->audio_device) == -1) {
		close(adev->fd);
		return -1;
	}

	return 0;
}

static int
audiodev_add(const char *dev, unsigned int unit)
{
	struct audiodev *adev;

	adev = calloc(1, sizeof(*adev));
	if (adev == NULL) 
		return -1;

	strlcpy(adev->xname, dev, sizeof(adev->xname));
	snprintf(adev->path, sizeof(adev->path) - 1, "/dev/%s", dev);
	adev->unit = unit;

	if (audiodev_getinfo(adev) == -1) {
		free(adev);
		return -1;
	}

#ifdef DEBUG
	printf("[%c] %s: %s\n", adev->defaultdev ? '*' : ' ',
	    adev->path, adev->audio_device.name);
#endif

	TAILQ_INSERT_TAIL(&audiodevlist, adev, next);

	return 0;
}

static void
audiodev_cb(void *args, const char *dev, unsigned int unit)
{
	audiodev_add(dev, unit);
}

int
audiodev_refresh(void)
{
	struct audiodev *adev;
	int fd, error;

	fd = open(DRVCTLDEV, O_RDWR);
	if (fd == -1) {
		perror("open " DRVCTLDEV);
		return -1;
	}

	while (!TAILQ_EMPTY(&audiodevlist)) {
		adev = TAILQ_FIRST(&audiodevlist);
		if (adev->fd != -1)
			close(adev->fd);
		TAILQ_REMOVE(&audiodevlist, adev, next);
		free(adev);
	}

	error = drvctl_foreach(fd, "audio", audiodev_cb, NULL);
	if (error == -1) {
		perror("drvctl");
		return -1;
	}

	close(fd);

	return 0;
}

unsigned int
audiodev_count(void)
{
	struct audiodev *adev;
	unsigned int n;

	n = 0;
	TAILQ_FOREACH(adev, &audiodevlist, next)
		++n;

	return n;
}

struct audiodev *
audiodev_get(unsigned int i)
{
	struct audiodev *adev;
	unsigned int n;

	n = 0;
	TAILQ_FOREACH(adev, &audiodevlist, next) {
		if (n == i)
			return adev;
		++n;
	}

	return NULL;
}

int
audiodev_set_default(struct audiodev *adev)
{
	char audiopath[PATH_MAX+1];
	char soundpath[PATH_MAX+1];
	char audioctlpath[PATH_MAX+1];
	char mixerpath[PATH_MAX+1];

	snprintf(audiopath, sizeof(audiopath) - 1,
	    _PATH_AUDIO "%u", adev->unit);
	snprintf(soundpath, sizeof(soundpath) - 1,
	    _PATH_SOUND "%u", adev->unit);
	snprintf(audioctlpath, sizeof(audioctlpath) - 1,
	    _PATH_AUDIOCTL "%u", adev->unit);
	snprintf(mixerpath, sizeof(mixerpath) - 1,
	    _PATH_MIXER "%u", adev->unit);

	unlink(_PATH_AUDIO);
	unlink(_PATH_SOUND);
	unlink(_PATH_AUDIOCTL);
	unlink(_PATH_MIXER);

	if (symlink(audiopath, _PATH_AUDIO) == -1) {
		perror("symlink " _PATH_AUDIO);
		return -1;
	}
	if (symlink(soundpath, _PATH_SOUND) == -1) {
		perror("symlink " _PATH_SOUND);
		return -1;
	}
	if (symlink(audioctlpath, _PATH_AUDIOCTL) == -1) {
		perror("symlink " _PATH_AUDIOCTL);
		return -1;
	}
	if (symlink(mixerpath, _PATH_MIXER) == -1) {
		perror("symlink " _PATH_MIXER);
		return -1;
	}

	return 0;
}
