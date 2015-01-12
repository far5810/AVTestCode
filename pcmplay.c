#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

#include <alsa/asoundlib.h>

void print_dB(long dB)
{
	printf("%li.%02lidB", dB / 100, (dB < 0 ? -dB : dB) % 100);
}

int convert_prange(int val, int min, int max)
{
	int range = max - min;
	int tmp;

	if (range == 0)
		return 0;
	val -= min;
	tmp = rint((double)val/(double)range * 100);
	return tmp;
}

const char *get_percent(int val, int min, int max)
{
	static char str[32];
	int p;
	
	p = convert_prange(val, min, max);
	sprintf(str, "%i [%i%%]", val, p);
	return str;
}

void snd_mypcm_info()
{
	int val;

	printf("ALSA library version: %s\n", SND_LIB_VERSION_STR);

	printf("\nPCM stream types:\n");
	for (val = 0; val <= SND_PCM_STREAM_LAST; val++)
		printf(" %s\n", snd_pcm_stream_name((snd_pcm_stream_t)val));

	printf("\nPCM access types:\n");
	for (val = 0; val <= SND_PCM_ACCESS_LAST; val++)
		printf(" %s\n", snd_pcm_access_name((snd_pcm_access_t)val));

	printf("\nPCM formats:\n");
	for (val = 0; val <= SND_PCM_FORMAT_LAST; val++)
		if (snd_pcm_format_name((snd_pcm_format_t)val) != NULL)
			printf(" %s (%s)\n", snd_pcm_format_name((snd_pcm_format_t)val),
					snd_pcm_format_description((snd_pcm_format_t)val));

	printf("\nPCM subformats:\n");
	for (val = 0; val <= SND_PCM_SUBFORMAT_LAST; val++)
		printf(" %s (%s)\n", snd_pcm_subformat_name((snd_pcm_subformat_t)val),
			snd_pcm_subformat_description((snd_pcm_subformat_t)val));

	printf("\nPCM states:\n");
	for (val = 0; val <= SND_PCM_STATE_LAST; val++)
		printf(" %s\n", snd_pcm_state_name((snd_pcm_state_t)val));
}

void snd_params_test()
{
	int rc;
	int dir;
	unsigned int val, val2;
	
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;

	/* Open PCM device for playback. */
	rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0) {
		fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
		exit(1);
	}

	/* Allocate a hardware parameters object. */
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	snd_pcm_hw_params_any(handle, params);

	/* Set the desired hardware parameters. */

	/* Interleaved mode */
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	/* Signed 16-bit little-endian format */
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

	/* Two channels (stereo) */
	snd_pcm_hw_params_set_channels(handle, params, 2);

	/* 44100 bits/second sampling rate (CD quality) */
	val = 44100 - 1;
	//val = 1411000;
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		exit(1);
	}

	/* Display information about the PCM interface */

	printf("PCM handle name = '%s'\n", snd_pcm_name(handle));

	printf("PCM state = %s\n", snd_pcm_state_name(snd_pcm_state(handle)));

	snd_pcm_hw_params_get_access(params,  (snd_pcm_access_t *) &val);
	printf("access type = %s\n", snd_pcm_access_name((snd_pcm_access_t)val));

	snd_pcm_hw_params_get_format(params, (snd_pcm_format_t *)&val);
	printf("format = '%s' (%s)\n", snd_pcm_format_name((snd_pcm_format_t)val),
		snd_pcm_format_description((snd_pcm_format_t)val));

	snd_pcm_hw_params_get_subformat(params, (snd_pcm_subformat_t *)&val);
	printf("subformat = '%s' (%s)\n", snd_pcm_subformat_name((snd_pcm_subformat_t)val),
		snd_pcm_subformat_description((snd_pcm_subformat_t)val));

	snd_pcm_hw_params_get_channels(params, &val);
	printf("channels = %d\n", val);

	snd_pcm_hw_params_get_rate(params, &val, &dir);
	printf("rate = %d bps\n", val);

	snd_pcm_hw_params_get_period_time(params, &val, &dir);
	printf("period time = %d us\n", val);

	snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	printf("period size = %d frames\n", (int)frames);

	snd_pcm_hw_params_get_buffer_time(params, &val, &dir);
	printf("buffer time = %d us\n", val);

	snd_pcm_hw_params_get_buffer_size(params, (snd_pcm_uframes_t *) &val);
	printf("buffer size = %d frames\n", val);

	snd_pcm_hw_params_get_periods(params, &val, &dir);
	printf("periods per buffer = %d frames\n", val);

	snd_pcm_hw_params_get_rate_numden(params, &val, &val2);
	printf("exact rate = %d/%d bps\n", val, val2);

	val = snd_pcm_hw_params_get_sbits(params);
	printf("significant bits = %d\n", val);
/*
	snd_pcm_hw_params_get_tick_time(params, &val, &dir);
	printf("tick time = %d us\n", val);
*/
	val = snd_pcm_hw_params_is_batch(params);
	printf("is batch = %d\n", val);

	val = snd_pcm_hw_params_is_block_transfer(params);
	printf("is block transfer = %d\n", val);

	val = snd_pcm_hw_params_is_double(params);
	printf("is double = %d\n", val);

	val = snd_pcm_hw_params_is_half_duplex(params);
	printf("is half duplex = %d\n", val);

	val = snd_pcm_hw_params_is_joint_duplex(params);
	printf("is joint duplex = %d\n", val);

	val = snd_pcm_hw_params_can_overrange(params);
	printf("can overrange = %d\n", val);

	val = snd_pcm_hw_params_can_mmap_sample_resolution(params);
	printf("can mmap = %d\n", val);

	val = snd_pcm_hw_params_can_pause(params);
	printf("can pause = %d\n", val);

	val = snd_pcm_hw_params_can_resume(params);
	printf("can resume = %d\n", val);

	val = snd_pcm_hw_params_can_sync_start(params);
	printf("can sync start = %d\n", val);

	snd_pcm_close(handle);
}

int show_selem(snd_mixer_t *handle, snd_mixer_selem_id_t *id, const char *space)
{
	snd_mixer_selem_channel_id_t chn;
	long pmin = 0, pmax = 0;
	long cmin = 0, cmax = 0;
	long pvol, cvol;
	int psw, csw;
	int pmono, cmono, mono_ok = 0;
	long db;
	snd_mixer_elem_t *elem;
	
	elem = snd_mixer_find_selem(handle, id);
	if (!elem) {
		printf("Mixer %s simple element not found", "default");
		return -ENOENT;
	}

	printf("%sCapabilities:", space);
	if (snd_mixer_selem_has_common_volume(elem)) {
		printf(" volume");
		if (snd_mixer_selem_has_playback_volume_joined(elem))
			printf(" volume-joined");
	} else {
		if (snd_mixer_selem_has_playback_volume(elem)) {
			printf(" pvolume");
			if (snd_mixer_selem_has_playback_volume_joined(elem))
				printf(" pvolume-joined");
		}
		if (snd_mixer_selem_has_capture_volume(elem)) {
			printf(" cvolume");
			if (snd_mixer_selem_has_capture_volume_joined(elem))
				printf(" cvolume-joined");
		}
	}
	if (snd_mixer_selem_has_common_switch(elem)) {
		printf(" switch");
		if (snd_mixer_selem_has_playback_switch_joined(elem))
			printf(" switch-joined");
	} else {
		if (snd_mixer_selem_has_playback_switch(elem)) {
			printf(" pswitch");
			if (snd_mixer_selem_has_playback_switch_joined(elem))
				printf(" pswitch-joined");
		}
		if (snd_mixer_selem_has_capture_switch(elem)) {
			printf(" cswitch");
			if (snd_mixer_selem_has_capture_switch_joined(elem))
				printf(" cswitch-joined");
			if (snd_mixer_selem_has_capture_switch_exclusive(elem))
				printf(" cswitch-exclusive");
		}
	}
	if (snd_mixer_selem_is_enum_playback(elem)) {
		printf(" penum");
	} else if (snd_mixer_selem_is_enum_capture(elem)) {
		printf(" cenum");
	} else if (snd_mixer_selem_is_enumerated(elem)) {
		printf(" enum");
	}
	printf("\n");
	if (snd_mixer_selem_is_enumerated(elem)) {
		int i, items;
		unsigned int idx;
		char itemname[40];
		items = snd_mixer_selem_get_enum_items(elem);
		printf("  Items:");
		for (i = 0; i < items; i++) {
			snd_mixer_selem_get_enum_item_name(elem, i, sizeof(itemname) - 1, itemname);
			printf(" '%s'", itemname);
		}
		printf("\n");
		for (i = 0; !snd_mixer_selem_get_enum_item(elem, i, &idx); i++) {
			snd_mixer_selem_get_enum_item_name(elem, idx, sizeof(itemname) - 1, itemname);
			printf("  Item%d: '%s'\n", i, itemname);
		}
		return 0; /* no more thing to do */
	}
	if (snd_mixer_selem_has_capture_switch_exclusive(elem))
		printf("%sCapture exclusive group: %i\n", space,
			   snd_mixer_selem_get_capture_group(elem));
	if (snd_mixer_selem_has_playback_volume(elem) ||
		snd_mixer_selem_has_playback_switch(elem)) {
		printf("%sPlayback channels:", space);
		if (snd_mixer_selem_is_playback_mono(elem)) {
			printf(" Mono");
		} else {
			int first = 1;
			for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++){
				if (!snd_mixer_selem_has_playback_channel(elem, chn))
					continue;
				if (!first)
					printf(" -");
				printf(" %s", snd_mixer_selem_channel_name(chn));
				first = 0;
			}
		}
		printf("\n");
	}
	if (snd_mixer_selem_has_capture_volume(elem) ||
		snd_mixer_selem_has_capture_switch(elem)) {
		printf("%sCapture channels:", space);
		if (snd_mixer_selem_is_capture_mono(elem)) {
			printf(" Mono");
		} else {
			int first = 1;
			for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++){
				if (!snd_mixer_selem_has_capture_channel(elem, chn))
					continue;
				if (!first)
					printf(" -");
				printf(" %s", snd_mixer_selem_channel_name(chn));
				first = 0;
			}
		}
		printf("\n");
	}
	if (snd_mixer_selem_has_playback_volume(elem) ||
		snd_mixer_selem_has_capture_volume(elem)) {
		printf("%sLimits:", space);
		if (snd_mixer_selem_has_common_volume(elem)) {
			snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
			snd_mixer_selem_get_capture_volume_range(elem, &cmin, &cmax);
			
			snd_mixer_selem_set_playback_volume(elem, 0, pmax);
			snd_mixer_selem_set_playback_volume(elem, 1, pmax);
			snd_mixer_selem_set_capture_volume(elem, 0, cmax);
			snd_mixer_selem_set_capture_volume(elem, 1, cmax);
			
			printf(" pppppp %li - %li", pmin, pmax);
			printf(" cccccc %li - %li", pmin, pmax);
		} else {
			if (snd_mixer_selem_has_playback_volume(elem)) {
				snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
				snd_mixer_selem_set_playback_volume(elem, 0, pmax);
				snd_mixer_selem_set_playback_volume(elem, 1, pmax);
				printf(" Playback %li - %li", pmin, pmax);
			}
			if (snd_mixer_selem_has_capture_volume(elem)) {
				snd_mixer_selem_get_capture_volume_range(elem, &cmin, &cmax);
				snd_mixer_selem_set_capture_volume(elem, 0, pmax);
				snd_mixer_selem_set_capture_volume(elem, 1, pmax);
				printf(" Capture %li - %li", cmin, cmax);
			}
		}
		printf("\n");
	}
	pmono = snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_MONO) &&
			(snd_mixer_selem_is_playback_mono(elem) || 
		 (!snd_mixer_selem_has_playback_volume(elem) &&
		  !snd_mixer_selem_has_playback_switch(elem)));
	cmono = snd_mixer_selem_has_capture_channel(elem, SND_MIXER_SCHN_MONO) &&
			(snd_mixer_selem_is_capture_mono(elem) || 
		 (!snd_mixer_selem_has_capture_volume(elem) &&
		  !snd_mixer_selem_has_capture_switch(elem)));
#if 0
	printf("pmono = %i, cmono = %i (%i, %i, %i, %i)\n", pmono, cmono,
			snd_mixer_selem_has_capture_channel(elem, SND_MIXER_SCHN_MONO),
			snd_mixer_selem_is_capture_mono(elem),
			snd_mixer_selem_has_capture_volume(elem),
			snd_mixer_selem_has_capture_switch(elem));
#endif
	if (pmono || cmono) {
		if (!mono_ok) {
			printf("%s%s:", space, "Mono");
			mono_ok = 1;
		}
		if (snd_mixer_selem_has_common_volume(elem)) {
			snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &pvol);
			printf(" %s", get_percent(pvol, pmin, pmax));
			if (!snd_mixer_selem_get_playback_dB(elem, SND_MIXER_SCHN_MONO, &db)) {
				printf(" [");
				print_dB(db);
				printf("]");
			}
		}
		if (snd_mixer_selem_has_common_switch(elem)) {
			snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_MONO, &psw);
			printf(" [%s]", psw ? "on" : "off");
		}
	}
	if (pmono && snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_MONO)) {
		int title = 0;
		if (!mono_ok) {
			printf("%s%s:", space, "Mono");
			mono_ok = 1;
		}
		if (!snd_mixer_selem_has_common_volume(elem)) {
			if (snd_mixer_selem_has_playback_volume(elem)) {
				printf(" Playback");
				title = 1;
				snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &pvol);
				printf(" %s", get_percent(pvol, pmin, pmax));
				if (!snd_mixer_selem_get_playback_dB(elem, SND_MIXER_SCHN_MONO, &db)) {
					printf(" [");
					print_dB(db);
					printf("]");
				}
			}
		}
		if (!snd_mixer_selem_has_common_switch(elem)) {
			if (snd_mixer_selem_has_playback_switch(elem)) {
				if (!title)
					printf(" Playback");
				snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_MONO, &psw);
				printf(" [%s]", psw ? "on" : "off");
			}
		}
	}
	if (cmono && snd_mixer_selem_has_capture_channel(elem, SND_MIXER_SCHN_MONO)) {
		int title = 0;
		if (!mono_ok) {
			printf("%s%s:", space, "Mono");
			mono_ok = 1;
		}
		if (!snd_mixer_selem_has_common_volume(elem)) {
			if (snd_mixer_selem_has_capture_volume(elem)) {
				printf(" Capture");
				title = 1;
				snd_mixer_selem_get_capture_volume(elem, SND_MIXER_SCHN_MONO, &cvol);
				printf(" %s", get_percent(cvol, cmin, cmax));
				if (!snd_mixer_selem_get_capture_dB(elem, SND_MIXER_SCHN_MONO, &db)) {
					printf(" [");
					print_dB(db);
					printf("]");
				}
			}
		}
		if (!snd_mixer_selem_has_common_switch(elem)) {
			if (snd_mixer_selem_has_capture_switch(elem)) {
				if (!title)
					printf(" Capture");
				snd_mixer_selem_get_capture_switch(elem, SND_MIXER_SCHN_MONO, &csw);
				printf(" [%s]", csw ? "on" : "off");
			}
		}
	}
	if (pmono || cmono)
		printf("\n");
	if (!pmono || !cmono) {
		for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++) {
			if ((pmono || !snd_mixer_selem_has_playback_channel(elem, chn)) &&
				(cmono || !snd_mixer_selem_has_capture_channel(elem, chn)))
				continue;
			printf("%s%s:", space, snd_mixer_selem_channel_name(chn));
			if (!pmono && !cmono && snd_mixer_selem_has_common_volume(elem)) {
				snd_mixer_selem_get_playback_volume(elem, chn, &pvol);
				printf(" %s", get_percent(pvol, pmin, pmax));
				if (!snd_mixer_selem_get_playback_dB(elem, chn, &db)) {
					printf(" [");
					print_dB(db);
					printf("]");
				}
			}
			if (!pmono && !cmono && snd_mixer_selem_has_common_switch(elem)) {
				snd_mixer_selem_get_playback_switch(elem, chn, &psw);
				printf(" [%s]", psw ? "on" : "off");
			}
			if (!pmono && snd_mixer_selem_has_playback_channel(elem, chn)) {
				int title = 0;
				if (!snd_mixer_selem_has_common_volume(elem)) {
					if (snd_mixer_selem_has_playback_volume(elem)) {
						printf(" Playback");
						title = 1;
						snd_mixer_selem_get_playback_volume(elem, chn, &pvol);
						printf(" %s", get_percent(pvol, pmin, pmax));
						if (!snd_mixer_selem_get_playback_dB(elem, chn, &db)) {
							printf(" [");
							print_dB(db);
							printf("]");
						}
					}
				}
				if (!snd_mixer_selem_has_common_switch(elem)) {
					if (snd_mixer_selem_has_playback_switch(elem)) {
						if (!title)
							printf(" Playback");
						snd_mixer_selem_get_playback_switch(elem, chn, &psw);
						printf(" [%s]", psw ? "on" : "off");
					}
				}
			}
			if (!cmono && snd_mixer_selem_has_capture_channel(elem, chn)) {
				int title = 0;
				if (!snd_mixer_selem_has_common_volume(elem)) {
					if (snd_mixer_selem_has_capture_volume(elem)) {
						printf(" Capture");
						title = 1;
						snd_mixer_selem_get_capture_volume(elem, chn, &cvol);
						printf(" %s", get_percent(cvol, cmin, cmax));
						if (!snd_mixer_selem_get_capture_dB(elem, chn, &db)) {
							printf(" [");
							print_dB(db);
							printf("]");
						}
					}
				}
				if (!snd_mixer_selem_has_common_switch(elem)) {
					if (snd_mixer_selem_has_capture_switch(elem)) {
						if (!title)
							printf(" Capture");
						snd_mixer_selem_get_capture_switch(elem, chn, &csw);
						printf(" [%s]", csw ? "on" : "off");
					}
				}
			}
			printf("\n");
		}
	}
	return 0;
}

int snd_set_volume(int val)
{
	snd_mixer_t *mixer;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *sid;

	snd_mixer_open(&mixer, 0);
	snd_mixer_attach(mixer, "dmixer");
	snd_mixer_selem_register(mixer, NULL, NULL);
	snd_mixer_load(mixer);

#if 0
	// "Headphone"	
	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, "Headphone Playback Volume");
	
	elem = snd_mixer_find_selem(mixer, sid);
	if(elem) {
		printf("=== Master\n");
		show_selem(mixer, sid, "    ");
		snd_mixer_selem_set_playback_switch_all(elem, 0);
		printf("==========\n");
		show_selem(mixer, sid, "    ");
		snd_mixer_selem_set_playback_volume_all(elem, 5);
		printf("==========\n");
		show_selem(mixer, sid, "    ");
	} else {
		printf("========== not found sid\n");
	
	}
#endif
	
#if 1
	elem = snd_mixer_first_elem(mixer);
	for( ; elem; elem = snd_mixer_elem_next(elem)) {
		if(snd_mixer_elem_get_type(elem) != SND_MIXER_ELEM_SIMPLE || 
			snd_mixer_selem_has_playback_volume(elem) == 0)
			continue;
		const char *pname = snd_mixer_selem_get_name(elem);
		printf("==%s\n", pname);
		
		if(strncmp(pname, "Mic", 3) != 0) continue;
		// snd_mixer_selem_set_playback_switch_all(elem, 0);
		
		snd_mixer_selem_id_alloca(&sid);
		snd_mixer_selem_id_set_index(sid, 0);
		snd_mixer_selem_id_set_name(sid, pname);
		
		show_selem(mixer, sid, "    ");
		
		// snd_mixer_selem_id_free(sid);
	}
#endif

#if 0
	elem = snd_mixer_find_selem(mixer, sid);
	if (elem != NULL) {
		long int vol, minvol, maxvol;
		snd_mixer_selem_get_playback_volume(elem, 1, &vol);
		snd_mixer_selem_get_playback_volume_range(elem, &minvol, &maxvol);
		printf("volume = %d, and range [%d,%d]\n", vol, minvol, maxvol);
		
		snd_mixer_selem_set_playback_switch_all(elem, 0);
		snd_mixer_selem_set_playback_volume_all(elem, val);
		
		snd_mixer_selem_get_playback_volume(elem, 1, &vol);
		
		snd_mixer_selem_set_playback_volume_range(elem, 0, 100);
		snd_mixer_selem_get_playback_volume_range(elem, &minvol, &maxvol);
		printf("volume = %d, and range [%d,%d]\n", vol, minvol, maxvol);
	}
#endif
	snd_mixer_close(mixer);
	return 0;
}


int snd_ctl_set_volume(int val)
{
	int err;   
	int orig_volume = 0;
	//unsigned int count;
	static snd_ctl_t *handle = NULL;   
	snd_ctl_elem_info_t *info;  
	snd_ctl_elem_id_t *id;  
	snd_ctl_elem_value_t *control;  
	//snd_ctl_elem_type_t type;

	snd_ctl_elem_info_alloca(&info);   
	snd_ctl_elem_id_alloca(&id);   
	snd_ctl_elem_value_alloca(&control);    

	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_id_set_name(id, "Headphone Playback Volume");
	// snd_ctl_elem_id_set_name(id, "PCM Playback Volume");
	// snd_ctl_elem_id_set_index(id, 1);  // "Mic Capture Volume" does not have index.   

	if ((err = snd_ctl_open(&handle, "default", 0)) < 0) { 
		printf("Control open error: %s\n", snd_strerror(err));
		return -1; 
	}  

	snd_ctl_elem_info_set_id(info, id);   
	if ((err = snd_ctl_elem_info(handle, info)) < 0) {  
		printf("Cannot find the given element from control: %s\n", snd_strerror(err));   
		snd_ctl_close(handle);
		return -1; 
	} 
	
	//type = snd_ctl_elem_info_get_type(info);   
	//count = snd_ctl_elem_info_get_count(info); 
	
	snd_ctl_elem_value_set_id(control, id);   

	if (!snd_ctl_elem_read(handle, control)) {  
		orig_volume = snd_ctl_elem_value_get_integer(control, 0); 
	} 

	if(val != orig_volume) {   
		printf("new_value(%d) orgin_value(%d)\n", val, orig_volume);
		snd_ctl_elem_value_set_integer(control, 0, (long)(val));
		snd_ctl_elem_value_set_integer(control, 1, (long)(val));
		
		if ((err = snd_ctl_elem_write(handle, control)) < 0) {  
			printf("Control element write error: %s\n", snd_strerror(err));   
			snd_ctl_close(handle);   
			return -1; 
		} 
	}  
	snd_ctl_close(handle);  
	return 0;
}

static const char *card = "default";
static int smixer_level = 0;
static struct snd_mixer_selem_regopt smixer_options;

int snd_mixer_info(void)
{
	int err;
	snd_ctl_t *handle;
	snd_mixer_t *mhandle;
	snd_ctl_card_info_t *info;
	snd_ctl_elem_list_t *clist;
	snd_ctl_card_info_alloca(&info);
	snd_ctl_elem_list_alloca(&clist);
	
	if ((err = snd_ctl_open(&handle, card, 0)) < 0) {
		printf("Control device %s open error: %s", card, snd_strerror(err));
		return err;
	}
	
	if ((err = snd_ctl_card_info(handle, info)) < 0) {
		printf("Control device %s hw info error: %s", card, snd_strerror(err));
		return err;
	}
	printf("Card %s '%s'/'%s'\n", card, snd_ctl_card_info_get_id(info),
	       snd_ctl_card_info_get_longname(info));
	printf("  Mixer name	: '%s'\n", snd_ctl_card_info_get_mixername(info));
	printf("  Components	: '%s'\n", snd_ctl_card_info_get_components(info));
	if ((err = snd_ctl_elem_list(handle, clist)) < 0) {
		printf("snd_ctl_elem_list failure: %s", snd_strerror(err));
	} else {
		printf("  Controls      : %i\n", snd_ctl_elem_list_get_count(clist));
	}
	snd_ctl_close(handle);
	if ((err = snd_mixer_open(&mhandle, 0)) < 0) {
		printf("Mixer open error: %s", snd_strerror(err));
		return err;
	}
	if (smixer_level == 0 && (err = snd_mixer_attach(mhandle, card)) < 0) {
		printf("Mixer attach %s error: %s", card, snd_strerror(err));
		snd_mixer_close(mhandle);
		return err;
	}
	if ((err = snd_mixer_selem_register(mhandle, smixer_level > 0 ? &smixer_options : NULL, NULL)) < 0) {
		printf("Mixer register error: %s", snd_strerror(err));
		snd_mixer_close(mhandle);
		return err;
	}
	err = snd_mixer_load(mhandle);
	if (err < 0) {
		printf("Mixer load %s error: %s", card, snd_strerror(err));
		snd_mixer_close(mhandle);
		return err;
	}
	printf("  Simple ctrls  : %i\n", snd_mixer_get_count(mhandle));
	snd_mixer_close(mhandle);
	return 0;
}

int main(int argc, char *argv[])
{	
	unsigned int val;
	int rc;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	
	// snd_params_test();

	/* Open PCM device for playback. */
	rc = snd_pcm_open(&handle, "plug:dmixer", SND_PCM_STREAM_PLAYBACK, 0);
	//rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0) {
		fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
		exit(1);
	}

	/* Allocate a hardware parameters object. */
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	snd_pcm_hw_params_any(handle, params);

	/* Set the desired hardware parameters. */

	/* Interleaved mode */
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	// snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_NONINTERLEAVED);

	/* Signed 16-bit little-endian format */
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

	/* Two channels (stereo) */
	snd_pcm_hw_params_set_channels(handle, params, 2);

	/* 44100 bits/second sampling rate (CD quality) */
	// val = 11025 - 1;
	// val = 22050 - 1;
	val = 44100 - 1;
	// val = 48000 - 1;
	// val = 64000 - 1;
	// val = 96000 - 1;
	
	char buffer[1024];
	
	snd_pcm_hw_params_set_rate_near(handle, params, &val, 0);
	
	unsigned period_time = 0;
	unsigned buffer_time = 0;
	//snd_pcm_uframes_t period_frames = 0;
	//snd_pcm_uframes_t buffer_frames = 0;
	
	snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time, 0);
	if (buffer_time > 500000) buffer_time = 500000;
	
	period_time = buffer_time >> 2;
	snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, 0);
	snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, 0);
	
	//frames = FRAMES;
	//snd_pcm_hw_params_set_period_size_near(handle, params, &frames, 0);

	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		snd_pcm_close(handle);
		exit(1);
	}

	/* Use a buffer large enough to hold one period */
	//snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	//snd_pcm_hw_params_get_period_time(params, &val, &dir);
	
	// snd_mypcm_info();
	// snd_mixer_info();
	snd_set_volume(2);
	snd_ctl_set_volume(10);
	
	do {
		rc = read(0, buffer, 256 * 4);
		if (rc == 0) {
			fprintf(stderr, "end of file on input\n");
			break;
		} else if (rc != 256 * 4) {
			fprintf(stderr, "short read: read %d bytes\n", rc);
		}
		
		rc = snd_pcm_writei(handle, buffer, 256);
		if (rc == -EPIPE) {
			/* EPIPE means underrun */
			fprintf(stderr, "underrun occurred\n");
			snd_pcm_prepare(handle);
		} else if (rc < 0) {
			fprintf(stderr, "error from writei: %s\n", snd_strerror(rc));
			break;
		} else if (rc != 256) {
			fprintf(stderr, "short write, write %d frames\n", rc);
		}
	} while (1);
	
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	
	return 0;
}










