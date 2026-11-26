"""Authored shadow controls validated against native at distances 100, 9000 and 9990."""
import json
import math
from pathlib import Path
import struct

def prepare(root, assets, distance):
    root=Path(root)
    for folder in ('models','materials','shaders'):
        (root/folder).mkdir(parents=True,exist_ok=True)
    stock=(Path(assets) / 'shaders/genericimage4.frag').read_text()
    fragment=stock[:stock.index('void main()')]+'''
float pointDepth(float d) {
 vec4 p=g_LFeature_ShadowPointProjection[0];
 return (-d*p.x+p.y)/(-d*p.z+p.w);
}
vec3 spotCoords(float d) {
 return CalculateProjectedCoords(vec3(0.0,0.0,d),g_LFeature_ShadowProjection[0]);
}
float pointCompare(float d) {
 vec4 p=CalculateProjectedCoordsPoint(vec3(0.0,0.0,d),g_LPoint_Origin[0].xyz,
  g_LFeature_ShadowPointProjection[0],g_LFeature_ShadowPointProjectionTransform[0]);
 return texSample2DCompare(SHADOW_ATLAS_SAMPLER,p.xy,p.z).r;
}
float spotCompare(float d) {
 vec3 p=spotCoords(d); vec4 atlas=g_LFeature_ShadowProjectionTransform[0];
 return texSample2DCompare(SHADOW_ATLAS_SAMPLER,p.xy*atlas.zw+atlas.xy,p.z).r;
}
vec3 encodeValue(float value) {
 float packed=floor(clamp((value+2.0)*0.25,0.0,1.0)*16777215.0);
 float red=floor(packed/65536.0); float green=floor((packed-red*65536.0)/256.0);
 return vec3(red,green,packed-red*65536.0-green*256.0)/255.0;
}
void main() {
 vec4 pointProjected=CalculateProjectedCoordsPoint(vec3(0,0,9990),g_LPoint_Origin[0].xyz,
  g_LFeature_ShadowPointProjection[0],g_LFeature_ShadowPointProjectionTransform[0]);
 vec3 spotProjected=spotCoords(9990.0); vec4 spotAtlas=g_LFeature_ShadowProjectionTransform[0];
 int cell=int(floor(v_TexCoord.x*24.0)); float value=0.0;
 if(cell==0) value=0.123456;
 if(cell==1) value=spotCoords(2.0).z;
 if(cell==2) value=spotCoords(2.0).x;
 if(cell==3) value=spotCoords(2.0).y;
 if(cell==4) value=g_LSpot_Direction[0].x;
 if(cell==5) value=g_LSpot_Direction[0].y;
 if(cell==6) value=g_LSpot_Direction[0].z;
 if(cell==7) value=pointProjected.x;
 if(cell==8) value=pointProjected.y;
 if(cell==9) value=pointProjected.z;
 if(cell==10) value=spotProjected.x*spotAtlas.z+spotAtlas.x;
 if(cell==11) value=spotProjected.y*spotAtlas.w+spotAtlas.y;
 if(cell==12) value=g_LFeature_ShadowPointProjectionTransform[0].x;
 if(cell==13) value=g_LFeature_ShadowPointProjectionTransform[0].y;
 if(cell==14) value=g_LFeature_ShadowPointProjectionTransform[0].z;
 if(cell==15) value=g_LFeature_ShadowPointProjectionTransform[0].w;
 if(cell==16) value=spotAtlas.x;
 if(cell==17) value=spotAtlas.y;
 if(cell==18) value=spotAtlas.z;
 if(cell==19) value=spotAtlas.w;
 if(cell==20) value=pointCompare(100.0);
 if(cell==21) value=pointCompare(9990.1);
 if(cell==22) value=spotCompare(100.0);
 if(cell==23) value=spotCompare(9990.1);
 vec3 color=encodeValue(value);
 if(v_TexCoord.y>0.55 && v_TexCoord.y<0.9) {
  float empty=texSample2DCompare(SHADOW_ATLAS_SAMPLER,vec2(v_TexCoord.x,(v_TexCoord.y-0.55)/0.35),0.0000000001).r;
  color=vec3(empty,empty,empty);
 }
 gl_FragColor=(v_TexCoord.y<0.1 || v_TexCoord.y>0.9) ? vec4(1,0,1,1) : vec4(color,1);
}
'''
    (root/'shaders/genericimage4.frag').write_text(fragment)
    (root/'materials/receiver.json').write_text(json.dumps({'passes':[{
        'shader':'genericimage4','textures':['util/white'],
        'combos':{'LIGHTING':1,'REFLECTION':0},'blending':'normal',
        'depthtest':'disabled','depthwrite':'disabled','cullmode':'nocull'}]}))
    (root/'models/receiver.json').write_text(json.dumps({'material':'materials/receiver.json','width':320,'height':180}))
    vertices=b''.join(struct.pack('<12f',x,y,9990.0,0,0,-1,1,0,0,1,0,0)
        for x,y in ((-2000,-2000),(2000,-2000),(2000,2000),(-2000,2000)))
    indices=struct.pack('<6H',0,1,2,0,2,3)
    (root/'models/caster.mdl').write_bytes(b'MDLV0013\0'+struct.pack('<3I',15,1,1)
        +b'materials/caster.json\0'+struct.pack('<2I',0,len(vertices))+vertices
        +struct.pack('<I',len(indices))+indices)
    (root/'materials/caster.json').write_text(json.dumps({'passes':[{
        'shader':'generic3','textures':['util/black'],'blending':'normal',
        'depthwrite':'enabled','depthtest':'enabled','cullmode':'nocull'}]}))
    scale=2*12*math.tan(math.radians(60)/2)/180
    scene={'camera':{'eye':'0 0 12','center':'0 0 0','up':'0 1 0'},
      'general':{'orthogonalprojection':None,'fov':60,'nearz':1,'farz':20000,
        'lightconfig':{'point':1,'pointshadow':1,'spot':1,'spotshadow':1},
        'clearcolor':'0 0 0','clearenabled':True,'bloom':False,'hdr':False,
        'camerafade':False,'cameraparallax':False,'ambientcolor':'0 0 0','skylightcolor':'0 0 0'},
      'objects':[
       {'id':1,'name':'Point source','light':'lpoint','origin':'0 0 0','radius':10000,
        'lightsourcesize':1,'intensity':1,'color':'1 1 1','castshadow':True,'solid':True},
       {'id':2,'name':'Spot source','light':'lspot','origin':'0 0 0','angles':'0 -1.57079632679 0',
        'radius':10000,'innercone':30,'outercone':45,'intensity':1,'color':'1 1 1',
        'castshadow':True,'solid':True},
       {'id':3,'name':'Far flat caster','model':'models/caster.mdl','origin':f'0 0 {distance-9990}', 'castshadow':True},
       {'id':4,'name':'Shadow contract panel','image':'models/receiver.json','size':'320 180',
        'origin':'0 0 0','scale':f'{scale:.12g} {scale:.12g} {scale:.12g}','castshadow':False,
        'visible':{'value':False,'script':'let ticks=0;export function update(v){return ++ticks>=12;}'}}
      ]}
    (root/'scene.json').write_text(json.dumps(scene))
    (root/'project.json').write_text(json.dumps({'type':'scene','file':'scene.json','title':'Shadow reverse depth contract'}))
    return scene

NAMES=['sentinel', 'spot_depth_d2', 'spot_u_d2', 'spot_v_d2', 'spot_direction_x', 'spot_direction_y', 'spot_direction_z', 'point_u_d9990', 'point_v_d9990', 'point_depth_d9990', 'spot_atlas_u_d9990', 'spot_atlas_v_d9990', 'point_transform_x', 'point_transform_y', 'point_transform_z', 'point_transform_w', 'spot_transform_x', 'spot_transform_y', 'spot_transform_z', 'spot_transform_w', 'point_compare_d100', 'point_compare_d9990_1', 'spot_compare_d100', 'spot_compare_d9990_1']
def decode(pixel):
 r,g,b=pixel
 return (r*65536+g*256+b)/16777215.0*4.0-2.0

def sample(image):
 image=image.convert('RGB')
 assert image.size==(320,180)
 ready=sum(r>240 and g<15 and b>240 for r,g,b in image.getdata())
 rows=[]
 for y in (54,126):
  values={name:decode(image.getpixel((round((i+.5)*320/24),y))) for i,name in enumerate(NAMES)}
  rows.append((abs(values['sentinel']-.123456),y,values))
 err,y,values=min(rows)
 atlas=image.crop((0,99,320,162) if y==54 else (0,18,320,81))
 black=sum(max(p)<10 for p in atlas.getdata());white=sum(min(p)>245 for p in atlas.getdata())
 return {'frame_complete':ready>=500 and err<1e-5,'ready_pixels':ready,
   'numeric_row':y,'numeric_values':values,'transport_error':err,
   'atlas_occupied_pixels':black,'atlas_empty_pixels':white,'atlas_denominator':atlas.width*atlas.height}
