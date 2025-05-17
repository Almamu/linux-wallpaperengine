# MDL Files
These seem to be simple model files containing the required information to render 3D objects.
They're also used in some 2D backgrounds (like 2879436369) for bones.

**NOTE**: This documentation page is not completed yet as there's still work left to do on the reverse engineering of this file format. For now just a structure, describing the file format is here

```
//
// 010 editor template
//
//
typedef struct {
    float x <fgcolor=cRed>;
    float y <fgcolor=cGreen>;
    float z <fgcolor=cBlue>;
} VECTOR3;

typedef struct {
    float x <fgcolor=cRed>;
    float y <fgcolor=cGreen>;
    float z <fgcolor=cBlue>;
    float w <fgcolor=cPurple>;
} VECTOR4;

typedef struct {
    float x <fgcolor=cRed>;
    float y <fgcolor=cGreen>;
} VECTOR2;

typedef struct {
    DWORD x <fgcolor=cRed>;
    DWORD y <fgcolor=cGreen>;
    DWORD z <fgcolor=cBlue>;
    DWORD w <fgcolor=cPurple>;
} BLENDINDICES;

typedef struct {
    VECTOR3 vertex <bgcolor=cRed>;
    BLENDINDICES blendindices;
    VECTOR4 blendweight;
    VECTOR2 uv <bgcolor=cBlue>;
} VERTEX;

typedef struct {
    WORD x;
    WORD y;
    WORD z;
} VERTEXINDICE;

typedef struct {
    CHAR header[];
    DWORD first;
    DWORD second;
    DWORD third;
    CHAR json[];
    DWORD fourth;
    DWORD fifth;
    DWORD vertexByteLength;
    VERTEX vertices[vertexByteLength / sizeof(VERTEX)];
    DWORD indicesByteLength;
    VERTEXINDICE indices[indicesByteLength / sizeof (VERTEXINDICE)] <bgcolor=cYellow>;
} MDLVHEADER;

typedef struct {
    BYTE tmp;
    DWORD type;
    DWORD unk1;
} BONEENTRYHEADER;

typedef struct {
    BONEENTRYHEADER header;
    DWORD entryByteLength;
    float v[entryByteLength / sizeof (float)];
    CHAR info[];
} BONEENTRY;

typedef struct {
    BONEENTRYHEADER header;
} BONE2ENTRY;

typedef struct {
    CHAR header[];
    DWORD mightBeByteLength;
    DWORD numberOfBones;
    BONEENTRY bones[numberOfBones]<optimize=false>;
    BONE2ENTRY unk[numberOfBones]<optimize=false>;
} MDLSHEADER;

typedef struct {
    CHAR header[];
} MDLAHEADER;

typedef struct {
    MDLVHEADER mdlv;
    MDLSHEADER mdls;
} MDLFILE;

LittleEndian ();
MDLFILE file;

// mdlv => vertices information
// mdls => skinning information
// mdla => animation information
```