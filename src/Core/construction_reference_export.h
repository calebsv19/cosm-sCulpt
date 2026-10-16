#pragma once
#include <stdbool.h>
#include <stddef.h>
bool ConstructionReference_Export(const char* collection,char* message,size_t capacity);
const char* ConstructionReference_LastDirectory(void);

bool ConstructionReference_OpenLast(void);
