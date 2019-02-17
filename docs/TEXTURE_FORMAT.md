# Texture storage format
Wallpaper engine uses a custom texture storage format converted by resourcecompiler.exe

## Header
| Information | Size | Default value |
|---|---|---|
| __File type indicator__ | 8 bytes | TEXV0005 |
| __Padding__ | 1 byte | Null terminator for string |
| __Extra file type indicator?__ | 8 bytes | TEXI0001 |
| __Padding__ | 1 byte | Null terminator for string |
| __Texture type__ | 1 byte | 4 => DXT5? 0 => JPEG? |
| __Unknown data__ | 15 bytes | To be reversed |
| __Width__ | 4 bytes | Image's width |
| __Height__ | 4 bytes | Image's height |
| __Unknown data__ | 5 bytes | To be reversed |
| __Container version__ | 8 bytes | TEXB0003 |
| __Padding__ | 1 byte | Null terminator for string |
| __Unknown data__ | 8 bytes | To be reversed |
| __Mip map levels__ | 4 bytes | The number of mipmaps stored for this texture |
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
