/*
 * Project Name: NickCollator
 *
 * Description:
 * This module enables your UnrealIRCd server to handle nick names across
 * various scripts and languages by using Unicode encoding for consistent
 * character handling.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * Links to external resources:
 * - Template for custom configuration blocks in UnrealIRCd:
 *   https://gitgud.malvager.net/Wazakindjes/unrealircd_mods
 * - ICU Project: https://icu.unicode.org/
 * - UnrealIRCd: https://www.unrealircd.org/
 */

#include "unrealircd.h"
#include <unicode/ucol.h>     // ICU for collation (sorting/comparing strings)
#include <unicode/ucnv.h>     // ICU for UTF-8 conversion
#include <unicode/ustring.h>  // ICU for handling Unicode strings
#include <unicode/unorm2.h>   // ICU for normalizing Unicode

#define MYCONF "nickcollator"

// Structure to hold groups of equivalent characters
struct mapping {
    char **equivalents;       // List of equivalent characters or strings
    int num_equivalents;      // How many equivalents are in this group
};

// Structure for holding all mappings, related information and configuration options
struct cfgstruct {
    struct mapping *mappings; // Array of all mappings
    int num_mappings;         // Number of groups in mappings
    unsigned short int got_mapping; // Indicates if mappings are defined in config
    int collator_strength;  // Hold collator strength option
};

static struct cfgstruct muhcfg; // Global configuration structure

// Module header - contains basic info about the module
ModuleHeader MOD_HEADER = {
    "third/nickcollator",      // Module name
    "0.2.0",                     // Version number
    "Block nick changes based on custom character mapping and/or ICU Collation",  // Description
    "MrVain",                  // Author's name
    "unrealircd-6",            // Compatible UnrealIRCd version
};

// Function prototypes (declarations of functions defined later)
void setcfg(void);
void freecfg(void);
int MODNAME_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int MODNAME_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
void apply_collator_mapping(UChar *u_str, int32_t length);
int compare_nicks(const char *nick1, const char *nick2);
CMD_OVERRIDE_FUNC(override_nick);

// Global ICU collator (used for comparing strings based on rules)
static UCollator *collator = NULL;

// Function to initialize the configuration with default values
void setcfg(void) {
    muhcfg.mappings = NULL;       // Set mappings to NULL (empty at start)
    muhcfg.num_mappings = 0;      // Set number of mappings to zero
    muhcfg.got_mapping = 0;       // Indicate that no mappings are yet defined
    muhcfg.collator_strength = -1;     // Default to "off" for collator strength
}

// Function to free memory used by configuration
void freecfg(void) {
    int i, j;
    // Free each equivalent string within each mapping
    for (i = 0; i < muhcfg.num_mappings; i++) {
        for (j = 0; j < muhcfg.mappings[i].num_equivalents; j++) {
            safe_free(muhcfg.mappings[i].equivalents[j]);
        }
        safe_free(muhcfg.mappings[i].equivalents); // Free the equivalents array
    }
    safe_free(muhcfg.mappings); // Free the mappings array itself
}

// Initialize ICU collator (for comparing strings)
static void collator_init(void) {
    UErrorCode status = U_ZERO_ERROR;

    if (muhcfg.collator_strength != -1) {   // only if collator should be used
        collator = ucol_open("", &status);  // Open a default collator
        if (U_FAILURE(status)) {            // Check if it failed
            config_error("ICU collator could not be initialized: %s", u_errorName(status));
        }
        ucol_setStrength(collator, muhcfg.collator_strength); // Set collation strength
    }
}

// Function to apply mappings to a Unicode string
void apply_collator_mapping(UChar *u_str, int32_t length) {
    UErrorCode status = U_ZERO_ERROR;

    // Go through all mappings and apply equivalents
    for (int i = 0; i < muhcfg.num_mappings; i++) {
        struct mapping *current_mapping = &muhcfg.mappings[i];
        for (int j = 0; j < current_mapping->num_equivalents; j++) {
            for (int k = 0; k < current_mapping->num_equivalents; k++) {
                if (j == k) continue;  // Skip if mapping to itself

                // Convert both source and target equivalents to Unicode
                UChar u_from[10];
                UChar u_to[10];

                // Convert both source and target equivalents to Unicode
                int32_t from_len, to_len;
                u_strFromUTF8(u_from, 10, &from_len, current_mapping->equivalents[j], -1, &status);
                u_strFromUTF8(u_to, 10, &to_len, current_mapping->equivalents[k], -1, &status);

                if (U_FAILURE(status)) {
                    continue; // Skip if conversion fails
                }

                // Look for occurrences of u_from in the input string and replace them with u_to
                UChar *pos = u_strstr(u_str, u_from);
                while (pos) {
                    // Shift the remaining part of the string to accommodate the replacement
                    u_memmove(pos + to_len, pos + from_len, u_strlen(pos + from_len) + 1);

                    // Replace by copying u_to into position of u_from
                    u_memcpy(pos, u_to, to_len);

                    // Move to the next occurrence
                    pos = u_strstr(pos + to_len, u_from);
                }
            }
        }
    }
}


// Function to compare two nicknames after applying mappings
int compare_nicks(const char *nick1, const char *nick2) {
    UChar u_nick1[100];
    UChar u_nick2[100];

    UErrorCode status = U_ZERO_ERROR;

    // Convert nicknames from UTF-8 to Unicode
    u_strFromUTF8(u_nick1, 100, NULL, nick1, -1, &status);
    u_strFromUTF8(u_nick2, 100, NULL, nick2, -1, &status);

    if (U_FAILURE(status)) {
        return 0; // Return 0 if conversion fails
    }

    // Apply mappings to both nicknames
    apply_collator_mapping(u_nick1, u_strlen(u_nick1));
    apply_collator_mapping(u_nick2, u_strlen(u_nick2));

    // Compare the nicknames without the ICU collator IF collator_strength !=off
    if (muhcfg.collator_strength == -1) {
        return u_strcmp(u_nick1, u_nick2) == 0 ? 0 : 1;
    }

    // Compare the nicknames using the ICU collator
    return ucol_strcoll(collator, u_nick1, -1, u_nick2, -1);
}

// Override function for /NICK command to enforce nickname checks
CMD_OVERRIDE_FUNC(override_nick) {
    const char *newnick = parv[1]; // Get new nickname from parameters

    // Check if the new nickname is equivalent to the current nickname
    if (compare_nicks(newnick, client->name) == 0) {
        // If the new nickname is equivalent to the current nickname, allow the change
        CALL_NEXT_COMMAND_OVERRIDE();
        return;
    }

    Client *acptr;

    // Check each client's nickname against the new nickname for collisions
    list_for_each_entry(acptr, &client_list, client_node) {
        if (compare_nicks(newnick, acptr->name) == 0) {
            // Send error if the nickname is already in use by someone else
            sendnumeric(client, ERR_NICKNAMEINUSE, newnick);
            return;
        }
    }

    // If no collision, continue with the original /NICK command
    CALL_NEXT_COMMAND_OVERRIDE();
}



// Test function for checking configuration
int MODNAME_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs) {
    int errors = 0;
    ConfigEntry *cep, *cep2;

    if (type != CONFIG_MAIN)
        return 0; // Only check at main config level

    if (!ce || !ce->name || strcmp(ce->name, MYCONF))
        return 0; // Skip if not our block

    // Check each item in the "nickcollator" block
    for (cep = ce->items; cep; cep = cep->next) {
        if (!strcmp(cep->name, "collator_strength")) {
            // Verify the collator strength value is valid
            if (strcasecmp(cep->value, "off") &&
                strcasecmp(cep->value, "primary") &&
                strcasecmp(cep->value, "secondary") &&
                strcasecmp(cep->value, "tertiary") &&
                strcasecmp(cep->value, "quaternary") &&
                strcasecmp(cep->value, "identical")) {
                config_error("%s:%i: Invalid collator strength value: %s",
                             cep->file->filename, cep->line_number, cep->value);
                errors++;
            }
        } else if (!strcmp(cep->name, "mapping")) {
            muhcfg.got_mapping = 1; // Indicate mappings are present

            // Check nested entries in "mapping" directive
            for (cep2 = cep->items; cep2; cep2 = cep2->next) {
                if (!cep2->name || !strlen(cep2->name)) {
                    config_error("%s:%i: mapping entry must be non-empty",
                                 cep2->file->filename, cep2->line_number);
                    errors++;
                    continue;
                }
            }
        }
    }

    *errs = errors;
    return errors ? -1 : 1;
}

// Function to load mappings from config
int MODNAME_configrun(ConfigFile *cf, ConfigEntry *ce, int type) {
    ConfigEntry *cep, *cep2;
    char *token;

    if (type != CONFIG_MAIN)
        return 0;

    if (!ce || !ce->name || strcmp(ce->name, MYCONF))
        return 0;

    // Loop through each item in the "nickcollator" block
    for (cep = ce->items; cep; cep = cep->next) {
        if (!strcmp(cep->name, "collator_strength")) {
            if (!strcasecmp(cep->value, "off")) {
                muhcfg.collator_strength = -1; // -1 = off
            } else if (!strcasecmp(cep->value, "primary")) {
                muhcfg.collator_strength = UCOL_PRIMARY;
            } else if (!strcasecmp(cep->value, "secondary")) {
                muhcfg.collator_strength = UCOL_SECONDARY;
            } else if (!strcasecmp(cep->value, "tertiary")) {
                muhcfg.collator_strength = UCOL_TERTIARY;
            } else if (!strcasecmp(cep->value, "quaternary")) {
                muhcfg.collator_strength = UCOL_QUATERNARY;
            } else if (!strcasecmp(cep->value, "identical")) {
                muhcfg.collator_strength = UCOL_IDENTICAL;
            } else {
                config_error("Invalid collator strength value: %s", cep->value);
                return -1;
            }
        }
        if (!strcmp(cep->name, "mapping")) {
            // Process nested entries in "mapping"
            for (cep2 = cep->items; cep2; cep2 = cep2->next) {
                if (!cep2->name || !strlen(cep2->name)) {
                    config_error("%s:%i: mapping entry must be non-empty", cep2->file->filename, cep2->line_number);
                    return -1;
                }

                // Allocate memory for new mapping group
                muhcfg.num_mappings++;
                muhcfg.mappings = realloc(muhcfg.mappings, sizeof(struct mapping) * muhcfg.num_mappings);
                if (!muhcfg.mappings) {
                    config_error("Memory allocation error");
                    return -1; // Error if memory allocation fails
                }

                struct mapping *current_mapping = &muhcfg.mappings[muhcfg.num_mappings - 1];
                current_mapping->num_equivalents = 0;
                current_mapping->equivalents = NULL;

                // Tokenize mapping values (e.g., "A, B")
                char *mapping_str = strdup(cep2->name); // Use `cep2->name`
                token = strtok(mapping_str, ", ");
                while (token) {
                    current_mapping->num_equivalents++;
                    current_mapping->equivalents = realloc(current_mapping->equivalents, sizeof(char *) * current_mapping->num_equivalents);
                    if (!current_mapping->equivalents) {
                        config_error("Memory allocation error");
                        safe_free(mapping_str);
                        return -1;
                    }
                    current_mapping->equivalents[current_mapping->num_equivalents - 1] = strdup(token);
                    token = strtok(NULL, ", ");
                }
                safe_free(mapping_str); // Free temporary string
            }
        }
    }
    return 1;
}

// UnrealIRCd-specific module functions
MOD_TEST() {
    memset(&muhcfg, 0, sizeof(muhcfg)); // Initialize configuration structure
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, MODNAME_configtest);
    HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, MODNAME_configrun);
    return MOD_SUCCESS;
}

MOD_INIT() {
    MARK_AS_GLOBAL_MODULE(modinfo); // Mark as global module
    setcfg();                       // Initialize configuration
    collator_init();                // Initialize ICU collator
    return MOD_SUCCESS;
}

MOD_LOAD() {
    // Override the /NICK command with our custom function
    if (!CommandOverrideAdd(modinfo->handle, "NICK", 0, override_nick)) {
        return MOD_FAILED;
    }
    return MOD_SUCCESS;
}

MOD_UNLOAD() {
    if (collator != NULL) {
        ucol_close(collator); // Close ICU collator
    }
    freecfg();               // Free configuration memory
    return MOD_SUCCESS;
}

// Additional memory management functions
void* safe_alloc_nick_memory(size_t size) {
    void *memory = safe_alloc(size);
    if (!memory) {
        config_error("Failed to allocate memory for nickname handling.");
        exit(EXIT_FAILURE);
    }
    return memory;
}

void safe_free_nick_memory(void *memory) {
    safe_free(memory); // Safely free memory
}
