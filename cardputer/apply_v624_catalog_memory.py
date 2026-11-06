from pathlib import Path
p=Path("Zaven_Festival_Kit_V6/V6_Model.h");s=p.read_text()
old="static Catalog catalogA,catalogB;"
new="""// Cardputer ADV: use one ~76KB catalog instead of two. Serialise UI refresh.
static Catalog catalogA;
static std::atomic<bool> catalogUpdating(false),uiCatalogReading(false);"""
assert s.count(old)==1;s=s.replace(old,new,1);p.write_text(s)
p=Path("Zaven_Festival_Kit_V6/V6_Network.h");s=p.read_text()
old=""" Catalog* old=activeCatalog.load();Catalog* target=old==&catalogA?&catalogB:&catalogA;
 target->~Catalog();
 new (target) Catalog(); // in-place reset; no giant stack temporary"""
new=""" catalogUpdating.store(true);
 while(uiCatalogReading.load())vTaskDelay(pdMS_TO_TICKS(1));
 Catalog* target=&catalogA;
 activeCatalog.store(nullptr);catReady.store(false);
 target->~Catalog();
 new (target) Catalog();"""
assert s.count(old)==1;s=s.replace(old,new,1)
for old,new in [
("if(!s.length()){netBusy.store(false);workerState.store(4);return;}","if(!s.length()){netBusy.store(false);workerState.store(4);catalogUpdating.store(false);return;}"),
("if(!total){netBusy.store(false);workerState.store(4);return;}","if(!total){netBusy.store(false);workerState.store(4);catalogUpdating.store(false);return;}"),
("activeCatalog.store(target);catReady.store(true);workerState.store(3);netBusy.store(false);","activeCatalog.store(target);catReady.store(true);workerState.store(3);netBusy.store(false);catalogUpdating.store(false);")
]:
 assert s.count(old)==1;s=s.replace(old,new,1)
old="""     int maxPage=0;
     for(int page=0;page<=maxPage && page<40;page++){"""
new="""     catalogUpdating.store(true);
     while(uiCatalogReading.load())vTaskDelay(pdMS_TO_TICKS(1));
     int maxPage=0;
     for(int page=0;page<=maxPage && page<40;page++){"""
assert s.count(old)==1;s=s.replace(old,new,1)
old="""     paletteVersion.fetch_add(1);
     // Persist to microSD before disconnecting: survives restarts."""
new="""     paletteVersion.fetch_add(1);
     catalogUpdating.store(false);
     // Persist to microSD before disconnecting: survives restarts."""
assert s.count(old)==1;s=s.replace(old,new,1)
p.write_text(s)
p=Path("Zaven_Festival_Kit_V6/V6_UI.h");s=p.read_text()
old="""static void tick(){
 M5Cardputer.update();handleKeyboard();synchronizeState();"""
new="""static void tick(){
 M5Cardputer.update();
 if(catalogUpdating.load()){delay(5);return;}
 uiCatalogReading.store(true);
 if(catalogUpdating.load()){uiCatalogReading.store(false);delay(1);return;}
 handleKeyboard();synchronizeState();"""
assert s.count(old)==1;s=s.replace(old,new,1)
old=""" render();delay(1);
}
}"""
new=""" render();uiCatalogReading.store(false);delay(1);
}
}"""
assert s.count(old)==1;s=s.replace(old,new,1)
p.write_text(s)
print("V6.2.4 optimized catalog RAM; same LCD display renderer and 256 effects/palettes")
