#include <string.h>

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "smap2.h"

#define NUMBUFSIZ       32

/* convert a table of Lua into smap */
static struct SMAP *tabletosmap(lua_State *lua, int index){
  
  struct SMAP *map = smap_init(DEFAULT_INITIAL_CAPACITY,
		DEFAULT_LOAD_FACTOR, DEFAULT_CONCURRENCY_LEVEL,
		DEFAULT_ENTRY_POOL_SIZE, 0);
  char knbuf[NUMBUFSIZ], vnbuf[NUMBUFSIZ];
  long nkey;
  char *v;
  struct PAIR pair;
  
  lua_pushnil(lua);
  while(lua_next(lua, index) != 0){
    char *kbuf = NULL;
    size_t ksiz = 0;
    switch(lua_type(lua, -2)){
    case LUA_TNUMBER:
      ksiz = sprintf(knbuf, "%lld", (long long)lua_tonumber(lua, -2));
      nkey = lua_tonumber(lua, -2);
      kbuf = knbuf;
	  SMAP_SET_NUM_PAIR(&pair, nkey, 0);
      break;
    case LUA_TSTRING:
      kbuf = lua_tolstring(lua, -2, &ksiz);
      SMAP_SET_STR_PAIR(&pair, kbuf, ksiz, NULL);
      break;
    }
	

    
	if(kbuf){
      char *vbuf = NULL;
      size_t vsiz = 0;
      switch(lua_type(lua, -1)){
      case LUA_TNUMBER:
        vsiz = sprintf(vnbuf, "%lld", (long long)lua_tonumber(lua, -1));
        v = (char *)(long long)lua_tonumber(lua, -1);
        if(vsiz > sizeof(vnbuf)) vsiz = sizeof(vnbuf);
        vbuf = vnbuf;
        
        break;
      case LUA_TSTRING:
        vbuf = lua_tolstring(lua, -1, &vsiz);
        v = vbuf;
        break;
      }
      
      pair.data = v;
      
      smap_insert(map, &pair, 0);
    }
    lua_pop(lua, 1);
  }
  lua_pop(lua, 1);
  return map;
}


/* convert SMAP into a table of Lua */
static void smaptotable(lua_State *lua, struct SMAP *map){
  struct PAIR pair;
  struct PAIR *p;
  char keybuf[SMAP_MAX_KEY_LEN + 1];

  int num = smap_get_elm_num(map);
  lua_createtable(lua, 0, num);

  const char *kbuf;
  int ksiz;

  for(p = smap_get_first(map, &pair, keybuf, 0); p != NULL; (p = smap_get_next(map, &pair, keybuf, 0)) != NULL){
    int vsiz;
    char *vbuf = (char *)SMAP_GET_VALUE(p);
    printf("key: %s, value: %s\n", SMAP_GET_STR_KEY(p), vbuf);
    lua_pushlstring(lua, vbuf, strlen(vbuf));
    lua_setfield(lua, -2, SMAP_GET_STR_KEY(p));
  }
}

int
main(void)
{
	lua_State *L;
	struct PAIR pair;
	int rc;
	
	struct SMAP *map = smap_init(DEFAULT_INITIAL_CAPACITY,
		DEFAULT_LOAD_FACTOR, DEFAULT_CONCURRENCY_LEVEL,
		DEFAULT_ENTRY_POOL_SIZE, 0);

	SMAP_SET_STR_PAIR(&pair, "a", 1, "test111");
	smap_insert(map, &pair, 0);
	SMAP_SET_STR_PAIR(&pair, "b", 1, "test222");
	smap_insert(map, &pair, 0);

	L = luaL_newstate();
	luaopen_base(L);
	luaopen_table(L);
	luaopen_string(L);
	if(luaL_loadfile(L, "./test.lua") || lua_pcall(L, 0, 0, 0)) {
		printf((lua_tostring(L, -1)));
	}
	lua_getglobal(L, "test");
	smaptotable(L, map);

    rc = lua_pcall( L,    //VMachine
                        1,    //Argument Count
                        2,    //Return Value Count
                        0);
    if (rc) {
    	printf("call failed! %s\n", lua_tostring(L, -1));
    }
  if (lua_isstring(L, -1) && lua_isnumber(L, -2))
    {
        printf("Ret_1(string): %s\n", lua_tostring(L, -1));
        printf("Rec_2(double): %f\n", lua_tonumber(L, -2));
    }
    else {
		printf("error: %s\n", lua_tostring(L, -1));
    }
        lua_pop(L,2);    //只需要清理Return Value，pcall调用的入栈参数会自动清理
    lua_close(L);
}

