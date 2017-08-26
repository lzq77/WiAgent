
#include "vap.h"

static struct vap_data *vap_first = NULL;
static struct vap_data *vap_last = NULL;

static u8 bssid_mask[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static char *bssid_file_path = "/sys/kernel/debug/ieee80211/phy0/ath9k/bssid_extra";

static void compute_bssid_mask(const u8 *hw, const u8 *bssid)
{
    // For each VAP, update the bssid mask to include
    // the common bits of all VAPs.
    int i = 0;
    for (; i < 6; i++)
    {
          bssid_mask[i] &= ~(hw[i] ^ bssid[i]);
    }
}

/*
 * This re-computes the BSSID mask for this node
 * using all the BSSIDs of the VAPs, and sets the
 * hardware register accordingly.
 */
static void reset_bssid_mask(const u8 *hw_addr)
{
    struct vap_data *vap_temp = vap_first;
    while (vap_temp !=NULL) {
        compute_bssid_mask(hw_addr, vap_temp->bssid);
        vap_temp = vap_temp->next;
    }
    // Update bssid mask register through debugfs
    FILE *debugfs_file = fopen(bssid_file_path, "w");

    if (debugfs_file!=NULL) {
        fprintf(debugfs_file,MACSTR"\n", MAC2STR(bssid_mask));
        fclose(debugfs_file);
    }
}

struct vap_data *wimaster_get_vap(const u8 *addr)
{
    struct vap_data *vap_temp = vap_first;

    while (vap_temp !=  NULL) {
        if (os_memcmp(vap_temp->addr, addr, ETH_ALEN) == 0) 
            return vap_temp;
        vap_temp = vap_temp->next;
    }
    return NULL;
}

static void wimaster_vap_list(void)
{
    int num = 0;
    struct vap_data *vap_temp = vap_first;

    wpa_printf(MSG_INFO, "\nwi vap list:\n");
    while (vap_temp) {
        wpa_printf(MSG_INFO, "%d. vap:"MACSTR" bssid:"
                MACSTR" ssid:%s\n", ++num, MAC2STR(vap_temp->addr),
                MAC2STR(vap_temp->bssid), vap_temp->ssid);
        vap_temp =  vap_temp->next;
    }

}

struct vap_data * wimaster_vap_add(const u8 *bss_addr,
                    const u8 *addr, const u8 *bssid, const char *ssid)
{
    struct vap_data *vap_temp;

    vap_temp = wimaster_get_vap(addr);
    if (vap_temp) 
        return vap_temp;

    vap_temp = (struct vap_data *)os_zalloc(sizeof(struct vap_data));
    if(vap_temp == NULL) {
        fprintf(stderr, "vap malloc failed, memory is not enough!\n");
        return NULL;
    }

    if (vap_first == NULL) {
        vap_first = vap_temp;
        vap_last = vap_first;
    }
    else {
        vap_last->next = vap_temp;
        vap_last = vap_temp;
    } 
    os_memcpy(vap_last->addr, addr, ETH_ALEN);
    os_memcpy(vap_last->bssid, bssid, ETH_ALEN);
    vap_last->ssid = (char *)os_zalloc(strlen(ssid) + 1);
    strcpy(vap_last->ssid, ssid);
    vap_last->next = NULL;

    wimaster_vap_list();

    reset_bssid_mask(bss_addr);

    return vap_last;
}

int wimaster_vap_remove(const u8 *bss_addr, const u8 *addr)
{
    struct vap_data *vap_temp = vap_first;
    struct vap_data *vap_previous = NULL;

    while (vap_temp !=  NULL) {
        if (os_memcmp(vap_temp->addr, addr, ETH_ALEN) == 0) {

            if(vap_previous) {
                vap_previous->next = vap_temp->next;
            }
            else {
                vap_first = vap_temp->next;
            }
            
            if (vap_temp->ssid) {
                os_free(vap_temp->ssid);
                vap_temp->ssid = NULL;
            }
            os_free(vap_temp);
            vap_temp = NULL;
            
            break;
        }
        
        vap_previous = vap_temp;
        vap_temp = vap_temp->next;

    }
    reset_bssid_mask(bss_addr);
    return 0;
}

void wimaster_for_each_vap(void (*cb)(struct vap_data *vap, void *ctx), void *ctx)
{
    struct vap_data *vap;
    for(vap = vap_first; vap; vap = vap->next) {
        cb(vap, ctx);
    }
}



