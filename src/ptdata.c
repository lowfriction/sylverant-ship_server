/*
    Sylverant Ship Server
    Copyright (C) 2012 Lawrence Sebald

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License version 3
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sylverant/debug.h>
#include <sylverant/mtwist.h>

#include "ptdata.h"
#include "subcmd.h"
#include "items.h"

#define MIN(x, y) (x < y ? x : y)

static int have_v2pt = 0;
static int have_v3pt = 0;

static pt_v2_entry_t v2_ptdata[4][10];
static pt_v3_entry_t v3_ptdata[2][4][10];

static const int tool_base[28] = {
    Item_Monomate, Item_Dimate, Item_Trimate,
    Item_Monofluid, Item_Difluid, Item_Trifluid,
    Item_Antidote, Item_Antiparalysis, Item_Sol_Atomizer,
    Item_Moon_Atomizer, Item_Star_Atomizer, Item_Telepipe,
    Item_Trap_Vision, Item_Monogrinder, Item_Digrinder,
    Item_Trigrinder, Item_Power_Material, Item_Mind_Material,
    Item_Evade_Material, Item_HP_Material, Item_TP_Material,
    Item_Def_Material, Item_Hit_Material, Item_Luck_Material,
    Item_Scape_Doll, Item_Disk_Lv01, Item_Photon_Drop,
    Item_NoSuchItem
};

#define EPSILON 0.001f

#if !defined(__BIG_ENDIAN__) && !defined(WORDS_BIGENDIAN)
#define BE16(x) (((x >> 8) & 0xFF) | ((x & 0xFF) << 8))
#define BE32(x) (((x >> 24) & 0x00FF) | \
                 ((x >>  8) & 0xFF00) | \
                 ((x & 0xFF00) <<  8) | \
                 ((x & 0x00FF) << 24))
#else
#define BE16(x) x
#define BE32(x) x
#endif

int pt_read_v2(const char *fn) {
    FILE *fp;
    uint8_t buf[4];
    int rv = 0, i, j;
    uint32_t offsets[40];
#if defined(__BIG_ENDIAN__) || defined(WORDS_BIGENDIAN)
    int k, l;
#endif

    /* Open up the file */
    if(!(fp = fopen(fn, "rb"))) {
        debug(DBG_ERROR, "Cannot open %s: %s\n", fn, strerror(errno));
        return -1;
    }

    /* Make sure that it looks like a sane AFS file. */
    if(fread(buf, 1, 4, fp) != 4) {
        debug(DBG_ERROR, "Error reading file: %s\n", strerror(errno));
        rv = -2;
        goto out;
    }

    if(buf[0] != 0x41 || buf[1] != 0x46 || buf[2] != 0x53 || buf[3] != 0x00) {
        debug(DBG_ERROR, "%s is not an AFS archive!\n", fn);
        rv = -3;
        goto out;
    }

    /* Make sure there are exactly 40 entries */
    if(fread(buf, 1, 4, fp) != 4) {
        debug(DBG_ERROR, "Error reading file: %s\n", strerror(errno));
        rv = -2;
        goto out;
    }

    if(buf[0] != 40 || buf[1] != 0 || buf[2] != 0 || buf[3] != 0) {
        debug(DBG_ERROR, "%s does not appear to be an ItemPT.afs file\n", fn);
        rv = -4;
        goto out;
    }

    /* Read in the offsets and lengths */
    for(i = 0; i < 40; ++i) {
        if(fread(buf, 1, 4, fp) != 4) {
            debug(DBG_ERROR, "Error reading file: %s\n", strerror(errno));
            rv = -2;
            goto out;
        }

        offsets[i] = (buf[0]) | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);

        if(fread(buf, 1, 4, fp) != 4) {
            debug(DBG_ERROR, "Error reading file: %s\n", strerror(errno));
            rv = -2;
            goto out;
        }

        if(buf[0] != 0x40 || buf[1] != 0x09 || buf[2] != 0 || buf[3] != 0) {
            debug(DBG_ERROR, "Invalid sized entry in ItemPT.afs!\n");
            rv = 5;
            goto out;
        }
    }

    /* Now, parse each entry... */
    for(i = 0; i < 4; ++i) {
        for(j = 0; j < 10; ++j) {
            if(fseek(fp, (long)offsets[i * 10 + j], SEEK_SET)) {
                debug(DBG_ERROR, "fseek error: %s\n", strerror(errno));
                rv = -2;
                goto out;
            }

            if(fread(&v2_ptdata[i][j], 1, sizeof(pt_v2_entry_t), fp) !=
               sizeof(pt_v2_entry_t)) {
                debug(DBG_ERROR, "Error reading file: %s\n", strerror(errno));
                rv = -2;
                goto out;
            }

            /* Swap entries, if we need to */
#if defined(__BIG_ENDIAN__) || defined(WORDS_BIGENDIAN)
            for(k = 0; k < 28; ++k) {
                for(l = 0; l < 10; ++l) {
                    v2_ptdata[i][j].tool_frequency[k][l] =
                        LE16(v2_ptdata[i][j].tool_frequency[k][l]);
                }
            }

            for(k = 0; k < 100; ++k) {
                for(l = 0; l < 2; ++l) {
                    v2_ptdata[i][j].enemy_meseta[k][l] = 
                        LE16(v2_ptdata[i][j].enemy_meseta[k][l]);
                }
            }

            for(k = 0; k < 10; ++k) {
                for(l = 0; l < 2; ++l) {
                    v2_ptdata[i][j].box_meseta[k][l] = 
                        LE16(v2_ptdata[i][j].box_meseta[k][l]);
                }
            }

            for(k = 0; k < 18; ++k) {
                v2_ptdata[i][j].pointers[k] = LE32(v2_ptdata[i][j].pointers[k]);
            }

            v2_ptdata[i][j].armor_level = LE32(v2_ptdata[i][j].armor_level);
#endif
        }
    }

    have_v2pt = 1;

out:
    fclose(fp);
    return rv;
}

int pt_read_v3(const char *fn) {
    FILE *fp;
    uint8_t buf[4];
    int rv = 0, i, j, k;
    uint32_t offsets[80];
#if !defined(__BIG_ENDIAN__) && !defined(WORDS_BIGENDIAN)
    int l, m;
#endif

    /* Open up the file */
    if(!(fp = fopen(fn, "rb"))) {
        debug(DBG_ERROR, "Cannot open %s: %s\n", fn, strerror(errno));
        return -1;
    }

    /* Read in the offsets and lengths for the Episode I & II data. */
    for(i = 0; i < 80; ++i) {
        if(fseek(fp, 32, SEEK_CUR)) {
            debug(DBG_ERROR, "fseek error: %s\n", strerror(errno));
            rv = -2;
            goto out;
        }

        if(fread(buf, 1, 4, fp) != 4) {
            debug(DBG_ERROR, "Error reading file: %s\n", strerror(errno));
            rv = -2;
            goto out;
        }

        /* The offsets are in 2048 byte blocks. */
        offsets[i] = (buf[3]) | (buf[2] << 8) | (buf[1] << 16) | (buf[0] << 24);
        offsets[i] <<= 11;

        if(fread(buf, 1, 4, fp) != 4) {
            debug(DBG_ERROR, "Error reading file: %s\n", strerror(errno));
            rv = -2;
            goto out;
        }

        if(buf[0] != 0 || buf[1] != 0 || buf[2] != 0x09 || buf[3] != 0xE0) {
            debug(DBG_ERROR, "Invalid sized entry in ItemPT.gsl!\n");
            rv = 5;
            goto out;
        }

        /* Skip over the padding. */
        if(fseek(fp, 8, SEEK_CUR)) {
            debug(DBG_ERROR, "fseek error: %s\n", strerror(errno));
            rv = -2;
            goto out;
        }
    }

    /* Now, parse each entry... */
    for(i = 0; i < 2; ++i) {
        for(j = 0; j < 4; ++j) {
            for(k = 0; k < 10; ++k) {
                if(fseek(fp, (long)offsets[i * 40 + j * 10 + k], SEEK_SET)) {
                    debug(DBG_ERROR, "fseek error: %s\n", strerror(errno));
                    rv = -2;
                    goto out;
                }

                if(fread(&v3_ptdata[i][j][k], 1, sizeof(pt_v3_entry_t), fp) !=
                   sizeof(pt_v3_entry_t)) {
                    debug(DBG_ERROR, "Error reading file: %s\n",
                          strerror(errno));
                    rv = -2;
                    goto out;
                }

                /* Swap entries, if we need to */
#if !defined(__BIG_ENDIAN__) && !defined(WORDS_BIGENDIAN)
                for(l = 0; l < 28; ++l) {
                    for(m = 0; m < 10; ++m) {
                        v3_ptdata[i][j][k].tool_frequency[l][m] =
                            BE16(v3_ptdata[i][j][k].tool_frequency[l][m]);
                    }
                }

                for(l = 0; l < 23; ++l) {
                    for(m = 0; m < 6; ++m) {
                        v3_ptdata[i][j][k].percent_pattern[l][m] =
                            BE16(v3_ptdata[i][j][k].percent_pattern[l][m]);
                    }
                }

                for(l = 0; l < 100; ++l) {
                    for(m = 0; m < 2; ++m) {
                        v3_ptdata[i][j][k].enemy_meseta[l][m] = 
                            BE16(v3_ptdata[i][j][k].enemy_meseta[l][m]);
                    }
                }

                for(l = 0; l < 10; ++l) {
                    for(m = 0; m < 2; ++m) {
                        v3_ptdata[i][j][k].box_meseta[l][m] = 
                            BE16(v3_ptdata[i][j][k].box_meseta[l][m]);
                    }
                }

                for(l = 0; l < 18; ++l) {
                    v3_ptdata[i][j][k].pointers[l] =
                        BE32(v3_ptdata[i][j][k].pointers[l]);
                }

                v3_ptdata[i][j][k].armor_level =
                    BE32(v3_ptdata[i][j][k].armor_level);
#endif
            }
        }
    }

    have_v3pt = 1;

out:
    fclose(fp);
    return rv;
}

int pt_v2_enabled(void) {
    return have_v2pt;
}

int pt_v3_enabled(void) {
    return have_v3pt;
}

/*
   Generate a random weapon, based off of data for PSOv2. This is a rather ugly
   process, so here's a "short" description of how it actually works (note, the
   code itself will probably end up being shorter than the description (at least
   in number of bytes), but I felt this was needed to explain the insanity
   below)...

   First, we have to decide what weapon types are actually going to be available
   to generate. This involves looking at three things, the weapon ratio for the
   weapon type in the PT data, the minimum rank information (also in the PT
   data) and the area we're generating for. If the weapon ratio is zero, we
   don't have to look any further (this is expected to be a somewhat rare
   occurance, hence why its actually checked second in the code). Next, take the
   minimum rank and add the area number to it (where 0 = Forest 1, 9 = Ruins 3
   boss areas use the area that comes immediately after them, except Falz, which
   uses Ruins 3). If that result is greater than or equal to zero, then we can
   actually generate this type of weapon, so we add it to the list of weapons we
   might generate, and add its ratio into the total.

   The next thing we have to do is to decide what rank of weapon will be
   generated for each valid weapon type. To do this, we need to look at three
   pieces of data again: the weapon type's minimum rank, the weapon type's
   upgrade floor value (once again, in the PT data), and the area number. If the
   minimum rank is greater than or equal to zero, start with that value as the
   rank that will be generated. Otherwise, start with 0. Next, calculate the
   effective area for the rest of the calculation. For weapon types where the
   minimum rank is greater than or equal to zero, this will be the area number
   itself. For those where the minimum rank is less than zero, this will be the
   sum of the minimum rank and the area. Next, take the upgrade floor value and
   subtract it from the effective area. If the result is greater than or equal
   to zero, add one to the weapon's rank. Continue doing this until the result
   is less than zero.

   From here, we can actually decide what weapon itself to generate. To do this,
   simply take sum of the chances of all valid weapon types and generate a
   random number between 0 and that sum (including 0, excluding the sum). Then,
   run through and compare with the chances that each type has until you figure
   out what type to generate. Shift the weapon rank that was calculated above
   into place with the weapon type value, and there you go.

   To pick a grind value, take the remainder from the upgrade floor/effective
   area calculation above, and use that as an index into the power patterns in
   the PT data (the second index, not the first one). If this value is greater
   than 3 for some reason, cap it at 3. The power paterns in here should be used
   to decide what grind value to use. Pick a random number between 0 and 100
   (not including 100, but including 0) and see where you end up in the array.
   Whatever index you end up on is the grind value to use.

   Note: This does not take percentages and elements into account at all. Those
   will come later...
*/
static int generate_weapon_v2(pt_v2_entry_t *ent, int area, uint32_t item[4]) {
    uint32_t rnd;
    int i, j = 0, wchance = 0, warea = 0;
    int wtypes[12] = { 0 }, wranks[12] = { 0 }, gptrn[12] = { 0 };

    item[0] = item[1] = item[2] = item[3] = 0;

    /* Go through each weapon type to see what ones we actually will have to
       work with right now... */
    for(i = 0; i < 12; ++i) {
        if((ent->weapon_minrank[i] + area) >= 0 && ent->weapon_ratio[i] > 0) {
            wtypes[j] = i;
            wchance += ent->weapon_ratio[i];

            if(ent->weapon_minrank[i] >= 0) {
                warea = area;
                wranks[j] = ent->weapon_minrank[i];
            }
            else {
                warea = ent->weapon_minrank[i] + area;
                wranks[j] = 0;
            }

            /* Sanity check... Make sure this is sane before we go to the loop
               below, since it will end up being an infinite loop if its not
               sane... */
            if(ent->weapon_upgfloor[i] <= 0) {
                debug(DBG_WARN, "Invalid v2 weapon upgrade floor value for "
                      "floor %d, weapon type %d. Please check your ItemPT.afs "
                      "file for validity!\n", area, i);
                return -1;
            }

            while((warea - ent->weapon_upgfloor[i]) >= 0) {
                ++wranks[j];
                warea -= ent->weapon_upgfloor[i];
            }

            gptrn[j] = MIN(warea, 3);
            ++j;
        }
    }

    /* Sanity check... This shouldn't happen! */
    if(!j) {
        debug(DBG_WARN, "No v2 weapon to generate on floor %d, please check "
              "your ItemPT.afs file for validity!\n", area);
        return -1;
    }

    /* Roll the dice! */
    rnd = genrand_int32() % wchance;
    for(i = 0; i < j; ++i) {
        if((rnd -= ent->weapon_ratio[wtypes[i]]) > wchance) {
            item[0] = ((wtypes[i] + 1) << 8) | (wranks[i] << 16);

            /* Save off the grind pattern to use... */
            warea = gptrn[i];
            break;
        }
    }

    /* Sanity check... Once again, this shouldn't happen! */
    if(!item[0]) {
        debug(DBG_WARN, "Generated invalid v2 weapon. Please report this "
              "error!\n");
        return -1;
    }

    /* Next up, determine the grind value. */
    rnd = genrand_int32() % 100;
    for(i = 0; i < 9; ++i) {
        if((rnd -= ent->power_pattern[i][warea]) > 100) {
            item[0] |= (i << 24);
            break;
        }
    }

    /* Sanity check... */
    if(i >= 9) {
        debug(DBG_WARN, "Invalid power pattern for floor %d, pattern number "
              "%d. Please check your ItemPT.afs for validity!\n", area, gptrn);
        return -1;
    }

    /* XXXX: Handle elements and percentages */
    return 0;
}

static uint32_t generate_tool(uint16_t freqs[28][10], int area) {
    uint32_t rnd = genrand_int32() % 10000;
    int i;

    for(i = 0; i < 28; ++i) {
        if((rnd -= LE16(freqs[i][area])) > 10000) {
            return tool_base[i];
        }
    }

    return Item_NoSuchItem;
}

static int generate_tech(uint8_t freqs[19][10], int8_t levels[19][20],
                         int area, uint32_t item[4]) {
    uint32_t rnd, tech, level;
    uint32_t t1, t2;
    int i;

    rnd = genrand_int32();
    tech = rnd % 1000;
    rnd /= 1000;

    for(i = 0; i < 19; ++i) {
        if((tech -= freqs[i][area]) > 1000) {
            t1 = levels[i][area << 1];
            t2 = levels[i][(area << 1) + 1];

            if(t1 == -1 || t1 > t2)
                return -1;

            if(t1 < t2)
                level = (rnd % ((t2 + 1) - t1)) + t1;
            else
                level = t1;

            item[1] = i;
            item[0] |= (level << 16);
            return 0;
        }
    }

    /* Shouldn't get here... */
    return -1;
}

/* Generate an item drop from the PT data. This version uses the v2 PT data set,
   and thus is appropriate for any version before PSOGC. */
int pt_generate_v2_drop(ship_client_t *c, lobby_t *l, void *r) {
    subcmd_itemreq_t *req = (subcmd_itemreq_t *)r;
    const pt_v2_entry_t *ent = &v2_ptdata[l->difficulty][l->section];
    uint8_t dar;
    uint32_t rnd;
    uint16_t t1, t2;
    uint32_t i[4];

    /* Make sure the PT index in the packet is sane */
    if(req->pt_index > 0x30)
        return -1;

    /* If the PT index is 0x30, this is a box, not an enemy! */
    if(req->pt_index == 0x30)
        return pt_generate_v2_boxdrop(c, l, r);

    dar = ent->enemy_dar[req->pt_index];

    /* See if the enemy is going to drop anything at all this time... */
    rnd = genrand_int32() % 100;

    if(rnd >= dar)
        /* Nope. You get nothing! */
        return 0;

    /* XXXX: For now, just drop meseta... We'll worry about the rest later. */
    t1 = LE16(ent->enemy_meseta[req->pt_index][0]);
    t2 = LE16(ent->enemy_meseta[req->pt_index][1]);
    if(t1 < t2)
        rnd = (genrand_int32() % ((t2 + 1) - t1)) + t1;
    else
        rnd = (uint32_t)t1;

    if(rnd) {
        i[0] = 4;
        i[1] = i[2] = 0;
        i[3] = rnd;

        return subcmd_send_lobby_item(l, req, i);
    }

    return 0;
}

int pt_generate_v2_boxdrop(ship_client_t *c, lobby_t *l, void *r) {
    subcmd_itemreq_t *req = (subcmd_itemreq_t *)r;
    pt_v2_entry_t *ent = &v2_ptdata[l->difficulty][l->section];
    uint16_t obj_id;
    map_object_t *obj;
    uint32_t rnd, t1, t2;
    int area;
    uint32_t item[4];
    float f1, f2;

    /* Make sure this is actually a box drop... */
    if(req->pt_index != 0x30)
        return -1;

    /* Grab the object ID and make sure its sane, then grab the object itself */
    obj_id = LE16(req->req);
    if(obj_id > l->map_objs->count) {
        debug(DBG_WARN, "Guildard %u requested drop from invalid box\n",
              c->guildcard);
        return -1;
    }

    obj = &l->map_objs->objs[obj_id];

    /* Figure out the area we'll be worried with */
    area = c->cur_area;

    /* Dragon -> Cave 1 */
    if(area == 11)
        area = 3;
    /* De Rol Le -> Mine 1 */
    else if(area == 12)
        area = 6;
    /* Vol Opt -> Ruins 1 */
    else if(area == 13)
        area = 8;
    /* Dark Falz -> Ruins 3 */
    else if(area == 14)
        area = 10;
    /* Everything after Dark Falz -> Ruins 3 */
    else if(area > 14)
        area = 10;
    /* Invalid areas... */
    else if(area == 0) {
        debug(DBG_WARN, "Guildcard %u requested box drop on Pioneer 2\n",
              c->guildcard);
        return -1;
    }

    /* Subtract one, since we want the index in the box_drop array */
    --area;

    /* See if the object is fixed-type box */
    t1 = LE32(obj->dword[0]);
    f1 = *((float *)&t1);
    if((obj->skin == LE32(0x00000092) || obj->skin == LE32(0x00000161)) &&
       f1 < 1.0f + EPSILON && f1 > 1.0f - EPSILON) {
        /* See if it is a fully-fixed item */
        t2 = LE32(obj->dword[1]);
        f2 = *((float *)&t2);

        if(f2 < 1.0f + EPSILON && f2 > 1.0f - EPSILON) {
            /* Drop the requested item */
            item[0] = BE32(obj->dword[2]);
            item[1] = item[2] = item[3] = 0;

            /* If its a stackable item, make sure to give it a quantity of 1 */
            if(item_is_stackable(item[0]))
                item[1] = (1 << 8);

            return subcmd_send_lobby_item(l, req, item);
        }

        t1 = BE32(obj->dword[2]);
        switch(t1 & 0xFF) {
            case 0:
                goto generate_weapon;

            case 1:
                /* XXXX: Generate a guard */
                return 0;

            case 3:
                goto generate_tool;

            case 4:
                goto generate_meseta;

            default:
                debug(DBG_WARN, "Invalid type detected from fixed-type box!\n");
                return 0;
        }
    }

    /* XXXX: Make sure we don't need to drop a rare */

    /* Generate an item, according to the PT data */
    rnd = genrand_int32() % 100;

    if((rnd -= ent->box_drop[BOX_TYPE_WEAPON][area]) > 100) {
generate_weapon:
        /* Generate a weapon */
        if(generate_weapon_v2(ent, area, item)) {
            return 0;
        }

        return subcmd_send_lobby_item(l, req, item);
    }
    else if((rnd -= ent->box_drop[BOX_TYPE_ARMOR][area]) > 100) {
        /* XXXX: Generate an armor */
        return 0;
    }
    else if((rnd -= ent->box_drop[BOX_TYPE_SHIELD][area]) > 100) {
        /* XXXX: Generate a shield */
        return 0;
    }
    else if((rnd -= ent->box_drop[BOX_TYPE_UNIT][area]) > 100) {
        /* XXXX: Generate a unit */
        return 0;
    }
    else if((rnd -= ent->box_drop[BOX_TYPE_TOOL][area]) > 100) {
generate_tool:
        /* Generate a tool */
        item[0] = generate_tool(ent->tool_frequency, area);

        /* Neither of these should happen, but just in case... */
        if(item[0] == Item_Photon_Drop || item[0] == Item_NoSuchItem) {
            debug(DBG_WARN, "Generated invalid v2 tool! Please check your "
                  "ItemPT.afs file for validity!\n");
            return 0;
        }

        /* Clear the rest of the item. */
        item[1] = item[2] = item[3] = 0;

        /* If its a stackable item, make sure to give it a quantity of 1 */
        if(item_is_stackable(item[0]))
            item[1] = (1 << 8);

        if(item[0] == Item_Disk_Lv01) {
            if(generate_tech(ent->tech_frequency, ent->tech_levels, area,
                             item)) {
                debug(DBG_WARN, "Generated invalid technique! Please check "
                      "your ItemPT.afs file for validity!\n");
                return 0;
            }
        }

        return subcmd_send_lobby_item(l, req, item);
    }
    else if((rnd -= ent->box_drop[BOX_TYPE_MESETA][area]) > 100) {
generate_meseta:
        /* Generate money! */
        t1 = ent->box_meseta[area][0];
        t2 = ent->box_meseta[area][1];

        if(t1 < t2)
            rnd = (genrand_int32() % ((t2 + 1) - t1)) + t1;
        else
            rnd = t1;

        if(rnd) {
            item[0] = 4;
            item[1] = item[2] = 0;
            item[3] = rnd;
            return subcmd_send_lobby_item(l, req, item);
        }

        return 0;
    }

    /* You get nothing! */
    return 0;
}

/* Generate an item drop from the PT data. This version uses the v3 PT data set.
   This function only works for PSOGC. */
int pt_generate_v3_drop(ship_client_t *c, lobby_t *l, void *r) {
    subcmd_itemreq_t *req = (subcmd_itemreq_t *)r;
    pt_v3_entry_t *ent = &v3_ptdata[l->episode - 1][l->difficulty][l->section];
    uint8_t dar;
    uint32_t rnd;
    uint16_t t1, t2;
    uint32_t i[4];

    dar = ent->enemy_dar[req->pt_index];

    /* See if the enemy is going to drop anything at all this time... */
    rnd = genrand_int32() % 100;

    if(rnd >= dar)
        /* Nope. You get nothing! */
        return 0;

    /* XXXX: For now, just drop meseta... We'll worry about the rest later. */
    t1 = ent->enemy_meseta[req->pt_index][0];
    t2 = ent->enemy_meseta[req->pt_index][1];
    if(t1 < t2)
        rnd = (genrand_int32() % (t2 - t1)) + t1;
    else
        rnd = (uint32_t)t1;

    if(rnd) {
        i[0] = 4;
        i[1] = i[2] = 0;
        i[3] = rnd;

        return subcmd_send_lobby_item(l, req, i);
    }

    return 0;
}

/* Generate an item drop from the PT data. This version uses the v3 PT data set.
   This function only works for PSOBB. */
int pt_generate_bb_drop(ship_client_t *c, lobby_t *l, void *r) {
    subcmd_bb_itemreq_t *req = (subcmd_bb_itemreq_t *)r;
    pt_v3_entry_t *ent;
    uint8_t dar;
    uint32_t rnd;
    uint16_t t1, t2;
    uint32_t i[4];
    item_t *item;
    int rv;

    /* XXXX: Handle Episode 4! */
    if(l->episode == 3)
        return 0;

    ent = &v3_ptdata[l->episode - 1][l->difficulty][l->section];
    dar = ent->enemy_dar[req->pt_index];

    /* See if the enemy is going to drop anything at all this time... */
    rnd = genrand_int32() % 100;

    if(rnd >= dar)
        /* Nope. You get nothing! */
        return 0;

    /* XXXX: For now, just drop meseta... We'll worry about the rest later. */
    t1 = ent->enemy_meseta[req->pt_index][0];
    t2 = ent->enemy_meseta[req->pt_index][1];
    if(t1 < t2)
        rnd = (genrand_int32() % (t2 - t1)) + t1;
    else
        rnd = (uint32_t)t1;

    if(rnd) {
        i[0] = 4;
        i[1] = i[2] = 0;
        i[3] = rnd;

        pthread_mutex_lock(&l->mutex);
        item = lobby_add_item_locked(l, i);
        rv = subcmd_send_bb_lobby_item(l, req, item);
        pthread_mutex_unlock(&l->mutex);

        return rv;
    }

    return 0;
}