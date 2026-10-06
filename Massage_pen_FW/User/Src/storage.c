#include "storage.h"
Storage_Settings Storage_Load(void) {
 return (Storage_Settings){.heat_level=STORAGE_DUMMY_LEVEL,.vibration_level=STORAGE_DUMMY_LEVEL};
}
