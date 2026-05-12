//
// Created by dunamis on 23/02/2026.
//

#ifndef SMARTDRIVE_PLATFORM_H
#define SMARTDRIVE_PLATFORM_H

#ifdef ARDUINO
    #include <stdint.h>
    #include <stdlib.h>
    #include <string.h>
    using ::size_t;
#else
    #include <cstdint>
    #include <cstdlib>
    #include <cstring>
    using std::size_t;
#endif


#endif //SMARTDRIVE_PLATFORM_H