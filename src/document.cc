#include "document.h"

namespace superfastmatch
{
  DocumentCursor::DocumentCursor(Registry* registry):
  registry_(registry){
    cursor_=registry->getDocumentDB()->cursor();
    cursor_->jump();
  };

  DocumentCursor::~DocumentCursor(){
    delete cursor_;
  }

  bool DocumentCursor::jumpFirst(){
    return cursor_->jump();
  }

  bool DocumentCursor::jumpLast(){
    return cursor_->jump_back();
  };

  bool DocumentCursor::jump(string& key){
    return cursor_->jump(key);
  };

  Document* DocumentCursor::getNext(){
    string key;
    if (cursor_->get_key(&key,true)){
      Document* doc = new Document(key,registry_);
      return doc;
    };
    return NULL;
  };

  Document* DocumentCursor::getPrevious(){
    return NULL;
  };

  uint32_t DocumentCursor::getCount(){
    return registry_->getDocumentDB()->count();
  };

  // TODO Inline everything! OR load docs for each cursor!
  void DocumentCursor::fill_list_dictionary(TemplateDictionary* dict,uint32_t doctype,uint32_t docid){
    if (getCount()==0){
      return;
    }
    TemplateDictionary* page_dict=dict->AddIncludeDictionary("PAGING");
    page_dict->SetFilename(PAGING);
    uint32_t count=0;
    Document* doc;
    char* key=new char[8];
    size_t key_length;
    uint32_t di;
    if (doctype!=0){
      uint32_t dt=kc::hton32(doctype);
      memcpy(key,&dt,4);
      cursor_->jump(key,4);
    }
    delete[] key;
    key=cursor_->get_key(&key_length,false);
    if (key!=NULL){
      memcpy(&di,key+4,4);
      di=kc::ntoh32(di);
      page_dict->SetValueAndShowSection("PAGE",toString(di),"FIRST");
    }
    if (docid!=0){
      di=kc::hton32(docid);
      memcpy(key+4,&di,4);
      cursor_->jump(key,8);
    }
    delete[] key;
    while (((doc=getNext())!=NULL)&&(count<registry_->getPageSize())){
      if ((doctype!=0) && (doctype!=doc->doctype())){
        break;
      }
      TemplateDictionary* doc_dict = dict->AddSectionDictionary("DOCUMENT");
      doc_dict->SetIntValue("DOC_TYPE",doc->doctype());
      doc_dict->SetIntValue("DOC_ID",doc->docid());
      doc_dict->SetValue("DOC_TITLE",doc->title());
      count++;
    }
    if (doc!=NULL){
      key=cursor_->get_key(&key_length,false);
      memcpy(&di,key+4,4);
      di=kc::ntoh32(di);
      page_dict->SetValueAndShowSection("PAGE",toString(di),"NEXT");
      delete[] key;
    }
    if ((doctype==0)&&(cursor_->jump_back())){
      key=cursor_->get_key(&key_length,false);
      memcpy(&di,key+4,4);
      di=kc::ntoh32(di);
      page_dict->SetValueAndShowSection("PAGE",toString(di),"LAST");
      delete[] key;
    }
  }

  Document::Document(const uint32_t doctype,const uint32_t docid,const char* content,Registry* registry):
  doctype_(doctype),docid_(docid),registry_(registry),key_(0),content_(0),clean_text_(0),content_map_(0),hashes_(0),unique_sorted_hashes_(0),bloom_(0)
  {
    char key[8];
    uint32_t dt=kc::hton32(doctype_);
    uint32_t di=kc::hton32(docid_);
    memcpy(key,&dt,4);
    memcpy(key+4,&di,4);
    key_ = new string(key,8);
    content_ = new std::string(content);
  }
  
  Document::Document(const uint32_t doctype,const uint32_t docid,Registry* registry):
  doctype_(doctype),docid_(docid),registry_(registry),key_(0),content_(0),clean_text_(0),content_map_(0),hashes_(0),unique_sorted_hashes_(0),bloom_(0)
  {
    char key[8];
    uint32_t dt=kc::hton32(doctype_);
    uint32_t di=kc::hton32(docid_);
    memcpy(key,&dt,4);
    memcpy(key+4,&di,4);
    key_ = new string(key,8);;
    content_ = new string();
    load();
  }
  
  Document::Document(string& key,Registry* registry):
  registry_(registry),key_(0),content_(0),clean_text_(0),content_map_(0),hashes_(0),unique_sorted_hashes_(0),bloom_(0)
  {
    key_= new string(key);
    memcpy(&doctype_,key.data(),4);
    memcpy(&docid_,key.data()+4,4);
    doctype_=kc::ntoh32(doctype_);
    docid_=kc::ntoh32(docid_);
    content_ = new string();
    load();
  }
  
  Document::~Document(){
    if (clean_text_!=0){
      delete clean_text_;
      clean_text_=0;
    }
    if (content_map_!=0){
      delete content_map_;
      content_map_=0;
    }
    if (hashes_!=0){
      delete hashes_;
      hashes_=0;
    }
    if (unique_sorted_hashes_!=0){
      delete unique_sorted_hashes_;
      unique_sorted_hashes_=0;
    }
    if (bloom_!=0){
      delete bloom_;
      bloom_=0; 
    }
    if(key_!=0){
      delete key_;
      key_=0;
    }
    if (content_!=0){
      delete content_;
      content_=0;
    }
  }

  bool Document::load(){
    string hashes;
    if (not registry_->getHashesDB()->get(*key_,&hashes)){
      return false;
    }
    uint32_t offset=0;
    uint64_t length;
    uint64_t hash;
    uint64_t previous=0;;
    offset+=kc::readvarnum(hashes.data()+offset,hashes.size()-offset,&length);
    hashes_=new hashes_vector();
    hashes_->reserve(length);
    bloom_=new hashes_bloom();
    while (offset<hashes.size()){
      offset+=kc::readvarnum(hashes.data()+offset,hashes.size()-offset,&hash);
      hashes_->push_back(hash+previous);
      bloom_->set((hash+previous)&0xFFFFFF);
      previous+=hash;
    }
    return registry_->getDocumentDB()->get(*key_,content_);
  }
  
  bool Document::save(){
    // Maximum size of hashes given maximum hash value
    char* h = new char[(hashes().size()*kc::sizevarnum(MAX_HASH))+kc::sizevarnum(hashes().size())];
    uint32_t offset=0;
    hash_t previous=0;
    // Write the length first so that the read vector can be sized correctly
    offset+=kc::writevarnum(h,hashes().size());
    // Write deltas, knowing that the hashes are sorted
    for (hashes_vector::const_iterator it=hashes().begin(),ite=hashes().end();it!=ite;++it){
      offset+=kc::writevarnum(h+offset,*it-previous);
      previous=*it;
    }
    bool success = registry_->getHashesDB()->cas(key_->data(),key_->size(),NULL,0,h,offset) && \
         registry_->getDocumentDB()->cas(key_->data(),key_->size(),NULL,0,content_->data(),content_->size()); 
    delete[] h;
    return success;
  }
  
  bool Document::remove(){
    return registry_->getDocumentDB()->remove(*key_) && registry_->getHashesDB()->remove(*key_);
  }
  
  Document::hashes_vector& Document::hashes(){
    if (hashes_==0){
      hashes_ = new hashes_vector();
      if (bloom_==0){
        bloom_ = new hashes_bloom();
      }
      uint32_t length = getCleanText().length()-registry_->getWindowSize();
      hash_t white_space = registry_->getWhiteSpaceHash(false);
      uint32_t window_size=registry_->getWindowSize();
      uint32_t white_space_threshold=registry_->getWhiteSpaceThreshold();
      hashes_->resize(length);
      hash_t hash;
      uint32_t i=0;
      string::const_iterator it=getCleanText().begin(),ite=getCleanText().end()-registry_->getWindowSize();
      for (;it!=ite;++it){
        // if ((count(it,it+white_space_threshold,' ')+count(it+window_size-white_space_threshold,it+window_size,' '))>white_space_threshold){
        if (count(it,it+window_size,' ')>white_space_threshold){
          hash=white_space;
        }else{
          hash=hashmurmur(&(*it),window_size+1);
        }
        (*hashes_)[i]=hash;
        bloom_->set(hash&0xFFFFFF);
        i++;
      }
    }
    return *hashes_;
  }
  
  Document::hashes_vector& Document::unique_sorted_hashes(){
    if (unique_sorted_hashes_==0){
      unique_sorted_hashes_=new hashes_vector(hashes());
      sort(unique_sorted_hashes_->begin(),unique_sorted_hashes_->end());
      hashes_vector::iterator end = unique (unique_sorted_hashes_->begin(), unique_sorted_hashes_->end());
      unique_sorted_hashes_->resize(end-unique_sorted_hashes_->begin());
    }
    return *unique_sorted_hashes_;
  }
  
  bool notAlphaNumeric(char c){
    return std::isalnum(c)==0;
  }

  string& Document::getCleanText(){
    if (clean_text_==0){
      clean_text_=new string(text().size(),' ');
      replace_copy_if(text().begin(),text().end(),clean_text_->begin(),notAlphaNumeric,' ');
      transform(clean_text_->begin(), clean_text_->end(), clean_text_->begin(), ::tolower);     
    }
    return *clean_text_;
  }
  
  Document::hashes_bloom& Document::bloom(){
    if (bloom_==0){
      hashes(); 
    }
    return *bloom_;
  }
  
  Document::content_map& Document::content(){
    if (content_map_==0){
      content_map_ = new content_map();
      kyototycoon::wwwformtomap(*content_,content_map_);
    }
    return *content_map_;
  }
  
  string& Document::text(){
    return content()["text"];
  }
  
  string& Document::title(){
    return content()["title"];  
  }
  
  string& Document::getKey(){
    return *key_;
  }
  
  uint64_t Document::index_key(){
    uint64_t index_key=doctype_;
    return index_key<<32 | docid_;
  }
    
  uint32_t Document::doctype(){
    return doctype_;
  }
      
  uint32_t Document::docid(){
    return docid_;
  } 
    
  ostream& operator<< (ostream& stream, Document& document) {
    stream << "Document(" << document.doctype() << "," << document.docid() << ")";
    return stream;
  }
  
  void Document::fill_document_dictionary(TemplateDictionary* dict){
    for (content_map::iterator it=content().begin();it!=content().end();it++){
      if (it->first!="text"){
        TemplateDictionary* meta_dict=dict->AddSectionDictionary("META");
        meta_dict->SetValue("KEY",it->first);
        meta_dict->SetValue("VALUE",it->second);
      }
    }
    dict->SetValue("TEXT",text());
    TemplateDictionary* association_dict=dict->AddIncludeDictionary("ASSOCIATION");
    association_dict->SetFilename(ASSOCIATION);
    // This belongs in Association Cursor
    // And need a key based Association Constructor
    kc::BasicDB::Cursor* cursor=registry_->getAssociationDB()->cursor();
    cursor->jump(getKey().data(),8);
    string next;
    string other_key;
    while((cursor->get_key(&next,true))&&(getKey().compare(next.substr(0,8))==0)){
      other_key=next.substr(8,8);
      Document other(other_key,registry_);
      Association association(registry_,this,&other);
      association.fill_item_dictionary(association_dict);
    }
    delete cursor;
  }
  
  bool operator< (Document& lhs,Document& rhs){
    if (lhs.doctype() == rhs.doctype()){
      return lhs.docid() < rhs.docid();
    } 
    return lhs.doctype() < rhs.doctype();
  }

}//namespace superfastmatch
