
#include "wi_bssid_mask.h"

static u8 bssid_mask[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

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
void set_bssid_mask(const char *file_name, const u8 *hw, const u8 *bssid)
{
    compute_bssid_mask(hw, bssid);
    
    // Update bssid mask register through debugfs
    FILE *debugfs_file = fopen (file_name, "w");

    if (debugfs_file!=NULL)
    {
        fprintf(debugfs_file,MACSTR"\n", MAC2STR(bssid_mask));
        fclose(debugfs_file);
    }
}
