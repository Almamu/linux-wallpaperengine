# Texture storage format
Wallpaper engine uses a custom texture storage format converted by resourcecompiler.exe

## Header
| Information | Size | Default value |
|---|---|---|
| __File type indicator__ | 8 bytes | TEXV0005 |
| __Padding__ | 1 byte | Null terminator for string |
| __Extra file type indicator?__ | 8 bytes | TEXI0001 |
| __Padding__ | 1 byte | Null terminator for string |
| __Texture type__ | 4 bytes | 0 => ARGB8888, 4 => DXT5, 6 => DXT3, 7 => DXT1, 8 => RG88, 9 => R8 |
| __Texture flags__ | 4 bytes | Flags for the texture (1 => Interpolation, 2 => ClampUVs, 4 => IsGif) |
| __Texture Width__ | 4 bytes | Texture's full width |
| __Texture Height__ | 4 bytes | Texture's full height |
| __Width__ | 4 bytes | Image's width |
| __Height__ | 4 bytes | Image's height |
| __Unknown data__ | 4 bytes | To be reversed |
| __Container version__ | 8 bytes | TEXB0003/TEXB0002/TEXB0001 |
| __Padding__ | 1 byte | Null terminator for string |
| __Texture information__ | x bytes | Varies depending on the container version |

## TEXB0003
| Information | Size | Default value |
|---|---|---|
| __Unknown data__ | 4 bytes | To be reversed |
| __Free image format__ | 4 bytes | The type of file this texture is based off the FREEIMAGE library |
| __Mip map levels__ | 4 bytes | The number of mipmap levels stored for this texture |
| __Mipmap entry__ | x bytes | See Mipmap entries |

## TEXB0002 and TEXB0001
| Information | Size | Default value |
|---|---|---|
| __Unknown data__ | 4 bytes | To be reversed |
| __Mip map levels__ | 4 bytes | The number of mipmap levels stored for this texture |
| __Mipmap entry__ | x bytes | See Mipmap entries |

## Mipmap entries
| Information | Size | Default value |
|---|---|---|
| __Width__ | 4 bytes | Mipmap's entry width |
| __Height__| 4 bytes | Mipmap's entry height |
| __Compression flag__ | 4 bytes | Indicates if the content is compressed or not |
| __Next image size__ | 4 bytes | Image block size (uncompressed) |
| __Next image size__ | 4 bytes | Image block size (compressed) |
| __Mipmap pixels__ | x bytes | Actual bitmap data in the format specified |
