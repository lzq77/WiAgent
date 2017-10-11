#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include <errno.h>
#include <string.h>
#include <pcap/pcap.h>
#include <pthread.h>
#include "../ap/ieee802_1x_defs.h"
#include "../utils/common.h"
#include "radiotap_iter.h"
#include "wicap.h"

pthread_mutex_t rssi_file_mutex = PTHREAD_MUTEX_INITIALIZER;
int is_filter_update = 0;
char filter_exp[1024];

static int fcshdr = 0;

static const struct radiotap_align_size align_size_000000_00[] = {
	[0] = { .align = 1, .size = 4, },
	[52] = { .align = 1, .size = 4, },
};

static const struct ieee80211_radiotap_namespace vns_array[] = {
	{
		.oui = 0x000000,
		.subns = 0,
		.n_bits = sizeof(align_size_000000_00),
		.align_size = align_size_000000_00,
	},
};

static const struct ieee80211_radiotap_vendor_namespaces vns = {
	.ns = vns_array,
	.n_ns = sizeof(vns_array)/sizeof(vns_array[0]),
};

static void print_radiotap_namespace(struct ieee80211_radiotap_iterator *iter)
{
	switch (iter->this_arg_index) {
	case IEEE80211_RADIOTAP_TSFT:
		printf("\tTSFT: %llu\n", le64toh(*(unsigned long long *)iter->this_arg));
		break;
	case IEEE80211_RADIOTAP_FLAGS:
		printf("\tflags: %02x\n", *iter->this_arg);
		break;
	case IEEE80211_RADIOTAP_RATE:
		printf("\trate: %lf\n", (double)*iter->this_arg/2);
		break;
	case IEEE80211_RADIOTAP_CHANNEL:
		printf("\tchannel frequency: %d\n", (u_int16_t)*iter->this_arg);
		break;
	case IEEE80211_RADIOTAP_FHSS:
	case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
		printf("\tdbm antsignal: %d\n", (int8_t)*iter->this_arg);
		break;
	case IEEE80211_RADIOTAP_DBM_ANTNOISE:
		printf("\tdbm antnoise: %d\n", (int8_t)*iter->this_arg);
		break;
	case IEEE80211_RADIOTAP_LOCK_QUALITY:
	case IEEE80211_RADIOTAP_TX_ATTENUATION:
	case IEEE80211_RADIOTAP_DB_TX_ATTENUATION:
	case IEEE80211_RADIOTAP_DBM_TX_POWER:
	case IEEE80211_RADIOTAP_ANTENNA:
	case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
		printf("\tdb antsignal: %d\n", (int8_t)*iter->this_arg);
		break;
	case IEEE80211_RADIOTAP_DB_ANTNOISE:
		printf("\tdb antnoise: %d\n", (int8_t)*iter->this_arg);
		break;
	case IEEE80211_RADIOTAP_TX_FLAGS:
		break;
	case IEEE80211_RADIOTAP_RX_FLAGS:
		if (fcshdr) {
			printf("\tFCS in header: %.8x\n",
				le32toh(*(uint32_t *)iter->this_arg));
			break;
		}
		printf("\tRX flags: %#.4x\n",
			le16toh(*(uint16_t *)iter->this_arg));
		break;
	case IEEE80211_RADIOTAP_RTS_RETRIES:
	case IEEE80211_RADIOTAP_DATA_RETRIES:
		break;
		break;
	default:
		printf("\tBOGUS DATA\n");
		break;
	}
}

static void print_test_namespace(struct ieee80211_radiotap_iterator *iter)
{
	switch (iter->this_arg_index) {
	case 0:
	case 52:
		printf("\t00:00:00-00|%d: %.2x/%.2x/%.2x/%.2x\n",
			iter->this_arg_index,
			*iter->this_arg, *(iter->this_arg + 1),
			*(iter->this_arg + 2), *(iter->this_arg + 3));
		break;
	default:
		printf("\tBOGUS DATA - vendor ns %d\n", iter->this_arg_index);
		break;
	}
}

static const struct radiotap_override overrides[] = {
	{ .field = 14, .align = 4, .size = 4, }
};

int save_packet(const pcap_t *pcap_handle, const struct pcap_pkthdr *header, 
        const u_char *packet, const char *file_name)
{
	pcap_dumper_t *output_file;
	
    output_file = pcap_dump_open(pcap_handle, file_name);
    if (output_file == NULL) {
        printf("Fail to save captured packet.\n");
        return -1;
    }

    pcap_dump((u_char *)output_file, header, packet);
    pcap_dump_close(output_file);
    
    return 0;
}

void got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)
{
    int err;
	struct ieee80211_radiotap_iterator iter;
    char *rssi_file_name = "/tmp/wiagent_rssi.hex";
    FILE *rssi_file_fd;
    int rssi;
    struct ieee80211_hdr *hdr;
    struct rssi_info rinfo;
    struct bpf_program fp;
    bpf_u_int32 netp;
    pcap_t *handle = (pcap_t *)args;

    err = ieee80211_radiotap_iterator_init(&iter, packet, 2014, &vns);
	if (err) {
		printf("malformed radiotap header (init returns %d)\n", err);
		return;
	}

    /**
     * Parsing captured data packet and print radiotap information.
     */
    while (!(err = ieee80211_radiotap_iterator_next(&iter))) {
	    if (iter.this_arg_index == IEEE80211_RADIOTAP_DBM_ANTSIGNAL) {
			rssi = (int8_t)iter.this_arg[0];
		}
    }

	if (err != -ENOENT) {
		printf("malformed radiotap data\n");
		return;
	}

    /**
     * Saving values to file, 
     * Using a mutex to keep files synchronized.
     */
    hdr =(struct ieee80211_hdr *)(packet + iter._max_length);
    os_memcpy(rinfo.da, hdr->addr1, 6);
    os_memcpy(rinfo.src, hdr->addr2, 6);
    os_memcpy(rinfo.bssid, hdr->addr3, 6);
    rinfo.rssi = rssi;

    pthread_mutex_lock(&rssi_file_mutex);
    rssi_file_fd = fopen(rssi_file_name, "ab+");
    fwrite(&rinfo, sizeof(struct rssi_info), 1, rssi_file_fd);
    fflush(rssi_file_fd);
    fclose(rssi_file_fd);
    pthread_mutex_unlock(&rssi_file_mutex);

    if(is_filter_update) {
        fprintf(stderr, "<-- rssi debug --> Updates libpcap filter expression, and compiles and applys again\n");
        /**
         * Updates libpcap filter expression, and compiles and applys again.
         */
        if (pcap_compile(handle, &fp, filter_exp, 0, netp) == -1) {
            fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
            return;
        }
        if (pcap_setfilter(handle, &fp) == -1) {
            fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(handle));
            return;
        }
        is_filter_update = 0;
    }

	return;

}

void* wicap(void *filt)
{
    pcap_t *handle;			/* Session handle */
    bpf_u_int32 netp;
    char errbuf[PCAP_ERRBUF_SIZE];	/* Error string */
    struct bpf_program fp;		/* The compiled filter */
    char *dev = "mon0";
    
    /* Open the session in promiscuous mode */
    handle = pcap_open_live(dev, BUFSIZ, 1, 100, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
        return;
    }

    /* Compile and apply the filter */
    if (pcap_compile(handle, &fp, filt, 0, netp) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
        return;
    }
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(handle));
        return;
    }
	
    pcap_loop(handle, -1, got_packet, handle);
	pcap_freecode(&fp);
	pcap_close(handle);
    
    return;
}
