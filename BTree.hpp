#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <fstream>

namespace sjtu {
template <class Key, class Value, class Compare = std::less<Key> >
class BTree {
public:
    typedef pair<Key, Value> value_type;
    typedef ssize_t offset_t;

    class iterator;
    class const_iterator;

private:
  static const int M=800;
  static const int L=500;
  /**basic note**/
  /**1.offset of tree meta is 0
   * 2.set array number as L+1 and M+1 for the convenience of insertion**/

  struct namestring{
      char *str;
      namestring(){
          str=new char[20];
      }
      ~namestring(){if(str!=NULL)delete str;}
      void setname(char *ch){
          int i=0;
          while(ch[i]!='\0'){
              str[i]=ch[i];
              i++;
          }
          str[i]='\0';
      }
  };
  struct treemeta{
      offset_t head;//first leaf
      offset_t tail;//tail of leaf
      offset_t root;//root
      size_t size;//size of bptree
      offset_t efo;//end of file
      treemeta():head(0),tail(0),root(0),size(0),efo(0){}
  };
  struct leafNode{
      offset_t offset;
      offset_t parent;
      offset_t prev,next;
      int inside;//numbers of pair in leaf
      value_type data[L+1];//预留一个
      leafNode():offset(0),parent(0),prev(0),next(0),inside(0){}
  };
  struct internalNode{
      offset_t offset;
      offset_t parent;
      offset_t children[M+1];//每个中间结点存M个孩子
      Key key[M+1];//每个中间结点存M个key（最小的也存下来）
      int inside;//number of children in internal node
      bool type;//0:child is internal node,1:child is leaf node
      internalNode():offset(0),parent(0),inside(0),type(0){
          for(int i=0;i<=M;i++) children[i]=0;
      }
  };

  FILE *fp;
  bool isopen;
  namestring fname;
  treemeta meta;
  /****************debug function*******************/
public:
  void printmeta(){
      std::cout<<meta.size<<' '<<meta.root<<' '<<meta.efo<<' '<<meta.head<<' '<<meta.tail<<' ';
      std::cout<<sizeof(treemeta)<<' '<<sizeof(leafNode)<<' '<<sizeof(internalNode)<<' ';
  }


  /****************** file IO ******************/
  bool file_already_exist;

  //if file exits, open it; if not, create and open it
  void openfile(){
      file_already_exist=1;
      if(isopen==0){
          fp=fopen(fname.str,"rb+");
          if(fp==nullptr){
              file_already_exist=0;//need to create an empty tree
              fp=fopen(fname.str,"w");
              fclose(fp);
              fp=fopen(fname.str,"rb+");
          }
          else {
              readfile(&meta,0,1,sizeof(treemeta));
          }
          isopen=true;
      }
  }
  void closefile(){
      if(isopen==true){
          fclose(fp);
          isopen=false;
      }
  }
  void readfile(void *dest,offset_t off,int num,int size){
      fseek(fp,off,SEEK_SET);
      fread(dest,size,num,fp);
  }
  void writefile(void *dest,offset_t off,int num,int size){
      fseek(fp,off,SEEK_SET);
      fwrite(dest,size,num,fp);
  }

  /******************** Utility Functions *************************/
  void create_empty_tree(){
      meta.size=0;
  //    std::cout<<'\n'<<meta.efo<<' ';
      meta.efo=sizeof(treemeta);
  //    std::cout<<"empty tree"<<meta.efo<<std::endl;

      internalNode root;
      leafNode leaf;

      meta.root=root.offset=meta.efo;
      meta.efo+=sizeof(internalNode);
      meta.head=meta.tail=leaf.offset=meta.efo;
      meta.efo+=sizeof(leafNode);

      root.parent=0;root.inside=1;root.type=1;

      root.children[0]=leaf.offset;
      leaf.parent=root.offset;

      leaf.next=leaf.prev=0;
      leaf.inside=0;

      writefile(&meta,0,1,sizeof(treemeta));
      writefile(&root,root.offset,1, sizeof(internalNode));
      writefile(&leaf,leaf.offset,1, sizeof(leafNode));
  }
  //give a key and find the leaf it should be in
  // return the offset of the leaf
  //always, the given offset is meta.root
  offset_t  find_to_leaf(const Key &k,offset_t off){
      internalNode p;
      readfile(&p,off,1, sizeof(internalNode));
      if(p.type==1){//children are leaf nodes
          int pos;
          for(pos=0;pos<p.inside;pos++){
              if(k<p.key[pos])break;
          }
          if(pos==0)return 0;//smaller than the smallest one in the node
          //imply that cannot find it
          return p.children[pos-1];
      }
      else{
          int pos;
          for(pos=0;pos<p.inside;pos++){
              if(k<p.key[pos])break;
          }
          if(pos==0)return 0;
          return find_to_leaf(k,p.children[pos-1]);
      }
  }
  pair<iterator,OperationResult> insert_leaf(leafNode &leaf,const Key &k,const Value &v){
      iterator ret;
      int pos;
      for(pos=0;pos<leaf.inside;pos++){
          if(k==leaf.data[pos].first)
              return pair<iterator,OperationResult>(iterator(),Fail);
          if(k<leaf.data[pos].first)break;//the first one bigger than k
      }
      for(int i=leaf.inside-1;i>=pos;i--){
          leaf.data[i+1].first=leaf.data[i].first;
          leaf.data[i+1].second=leaf.data[i].second;
      }
      leaf.data[pos].first=k;leaf.data[pos].second=v;

      ++leaf.inside;++meta.size;

      ret.bt=this;ret.offset=leaf.offset;ret.index=pos;
      writefile(&meta,0,1,sizeof(treemeta));
      if(leaf.inside<=L)writefile(&leaf,leaf.offset,1,sizeof(leafNode));
      else split_leaf(leaf,ret,k);//here,the iterator,the leaf node can be modified
      return pair<iterator,OperationResult>(ret,Success);
  }

  void split_leaf(leafNode &leaf,iterator &it,const Key &k){
      leafNode newleaf;

      newleaf.inside=leaf.inside-(leaf.inside>>1);
      leaf.inside=leaf.inside>>1;
      newleaf.offset=meta.efo;
      meta.efo+=sizeof(leafNode);
      newleaf.parent=leaf.parent;

      for(int i=0;i<newleaf.inside;i++){
          newleaf.data[i].first=leaf.data[i+leaf.inside].first;
          newleaf.data[i].second=leaf.data[i+leaf.inside].second;
          if(newleaf.data[i].first==k){
              it.offset=newleaf.offset;
              it.index=i;
          }
      }

      newleaf.next=leaf.next;
      newleaf.prev=leaf.offset;
      leaf.next=newleaf.offset;
      leafNode nextleaf;
      if(newleaf.next==0)meta.tail=newleaf.offset;
      else{
          readfile(&nextleaf,newleaf.next,1,sizeof(leafNode));
          nextleaf.prev=newleaf.offset;
          writefile(&nextleaf,nextleaf.offset,1,sizeof(leafNode));
      }

      writefile(&leaf,leaf.offset,1, sizeof(leafNode));
      writefile(&newleaf,newleaf.offset,1, sizeof(leafNode));
      writefile(&meta,0,1,sizeof(treemeta));

      internalNode father;
      readfile(&father,leaf.parent,1,sizeof(internalNode));
      insert_node(father,newleaf.data[0].first,newleaf.offset);
  }

  void insert_node(internalNode &node,const Key &k,offset_t child_off){
      int pos;
      for(pos=0;pos<node.inside;pos++){
          if(k<node.key[pos])break;//the first bigger one
      }
      for(int i=node.inside-1;i>=pos;i--){
          node.key[i+1]=node.key[i];
          node.children[i+1]=node.children[i];
      }
      node.key[pos]=k;node.children[pos]=child_off;
      ++node.inside;
      if(node.inside<=M)writefile(&node,node.offset,1,sizeof(internalNode));
      else split_node(node);
  }

  void split_node(internalNode &node){
      internalNode newnode;

      newnode.inside=node.inside-(node.inside>>1);
      node.inside=node.inside>>1;
      newnode.parent=node.parent;
      newnode.type=node.type;
      newnode.offset=meta.efo;
      meta.efo+= sizeof(internalNode);

      for(int i=0;i<newnode.inside;i++){
          newnode.key[i]=node.key[i+node.inside];
          newnode.children[i]=node.children[i+node.inside];
      }

      //update children's parent
      leafNode leaf;
      internalNode internal;
      for(int i=0;i<newnode.inside;i++){
          if(newnode.type==1){
              readfile(&leaf,newnode.children[i],1,sizeof(leafNode));
              leaf.parent=newnode.offset;
              writefile(&leaf,leaf.offset,1,sizeof(leafNode));
          }
          else{
              readfile(&internal,newnode.children[i],1,sizeof(internalNode));
              internal.parent=newnode.offset;
              writefile(&internal,internal.offset,1,sizeof(internalNode));
          }
      }

      if(node.offset==meta.root){//change root
          internalNode newroot;

          newroot.parent=0;newroot.type=0;
          newroot.offset=meta.efo;
          meta.efo+=sizeof(internalNode);
          newroot.inside=2;
          newroot.key[0]=node.key[0];
          newroot.children[0]=node.offset;
          newroot.key[1]=newnode.key[0];
          newroot.children[1]=newnode.offset;

          node.parent=newroot.offset;newnode.parent=newroot.offset;
          meta.root=newroot.offset;

          writefile(&meta,0,1,sizeof(treemeta));
          writefile(&node,node.offset,1,sizeof(internalNode));
          writefile(&newnode,newnode.offset,1,sizeof(internalNode));
          writefile(&newroot,newroot.offset,1,sizeof(internalNode));
      }
      else{
          writefile(&meta,0,1,sizeof(treemeta));
          writefile(&node,node.offset,1,sizeof(internalNode));
          writefile(&newnode,newnode.offset,1,sizeof(internalNode));

          internalNode father;
          readfile(&father,node.parent,1,sizeof(internalNode));
          insert_node(father,newnode.key[0],newnode.offset);
      }
  }


public:
  // Default Constructor and Copy Constructor
  BTree(char *s="mybpt") {
      fname.setname(s);
      fp=NULL;
      openfile();
      if(file_already_exist==0)create_empty_tree();
  }
  BTree(const BTree& other) {
  }
  BTree& operator=(const BTree& other) {
    // Todo Assignment
  }
  ~BTree() {
    closefile();
  }
  // Insert: Insert certain Key-Value into the database
  // Return a pair, the first of the pair is the iterator point to the new
  // element, the second of the pair is Success if it is successfully inserted
  pair<iterator, OperationResult> insert(const Key& key, const Value& value) {
      offset_t leaf_off=find_to_leaf(key,meta.root);//find the key should in which leaf
      leafNode leaf;

      if(meta.size==0||leaf_off==0){//the key is the smallest
          readfile(&leaf,meta.head,1,sizeof(leafNode));
          pair<iterator,OperationResult> ret=insert_leaf(leaf,key,value);
          if(ret.second==Fail)return ret;
          offset_t off=leaf.parent;
          internalNode node;
          while(off!=0){
              readfile(&node,off,1, sizeof(internalNode));
              node.key[0]=key;
              writefile(&node,off,1,sizeof(internalNode));//写回去
              off=node.parent;
          }
          return ret;
      }
      readfile(&leaf,leaf_off,1,sizeof(leafNode));
      pair<iterator,OperationResult> ret=insert_leaf(leaf,key,value);
      return ret;
  }
  // Erase: Erase the Key-Value
  // Return Success if it is successfully erased
  // Return Fail if the key doesn't exist in the database
  OperationResult erase(const Key& key) {
    // TODO erase function
    return Fail;  // If you can't finish erase part, just remaining here.
  }
  // Return a iterator to the beginning
  iterator begin() {
      return iterator(this,meta.head,0);
  }
  const_iterator cbegin() const {
      return const_iterator(this,meta.head,0);
  }
  // Return a iterator to the end(the next element after the last)
  iterator end() {
      leafNode tail;
      readfile(&tail,meta.tail,1,sizeof(leafNode));
      return iterator(this,meta.tail,tail.inside);
  }
  const_iterator cend() const {
      leafNode tail;
      readfile(&tail,meta.tail,1,sizeof(leafNode));
      return const_iterator(this,meta.tail,tail.inside);
  }
  // Check whether this BTree is empty
  bool empty() const {return meta.size==0;}
  // Return the number of <K,V> pairs
  size_t size() const {return meta.size;}
  // Clear the BTree
  void clear() {
      fp=fopen(fname.str,"w");//empty the file
      fclose(fp);
      openfile();
      create_empty_tree();
  }
  /**
   * Returns the number of elements with key
   *   that compares equivalent to the specified argument,
   * The default method of check the equivalence is !(a < b || b > a)
   */
  size_t count(const Key& key) const {
  }
  /**
   * Finds an element with key equivalent to key.
   * key value of the element to search for.
   * Iterator to an element with key equivalent to key.
   *   If no such element is found, past-the-end (see end()) iterator is
   * returned.
   */
  iterator find(const Key& key) {
      offset_t leaf_off=find_to_leaf(key,meta.root);
      if(leaf_off==0)return end();//not found
      leafNode leaf;
      readfile(&leaf,leaf_off,1,sizeof(leafNode));
      for(int i=0;i<leaf.inside;i++){
          if(leaf.data[i].first==key)return iterator(this,leaf_off,i);
      }
      return end();
  }
  const_iterator find(const Key& key) const {
      offset_t leaf_off=find_to_leaf(key,meta.root);
      if(leaf_off==0)return cend();//not found
      leafNode leaf;
      readfile(&leaf,leaf_off,1,sizeof(leafNode));
      for(int i=0;i<leaf.inside;i++){
          if(leaf.data[i].first==key)return const_iterator(this,leaf_off,i);
      }
      return cend();
  }
  Value at(const Key &k){
      iterator it=find(k);
      leafNode leaf;
      if(it==end())throw index_out_of_bound();
      readfile(&leaf,it.offset,1,sizeof(leafNode));
      return leaf.data[it.index].second;
  }

    class iterator {
      friend class BTree;
    private:
        offset_t offset;//the offset of leafnode
        int index;
        BTree *bt;
    public:
        bool modify(const Key& key){

        }

        iterator(BTree *t=NULL,offset_t off=0,int in=0) {
            bt=t;index=in;offset=off;
        }
        iterator(const iterator& other) {
            bt=other.bt;
            offset=other.offset;
            index=other.index;
        }
        Value getvalue(){
            leafNode p;
            bt->readfile(&p,offset,1, sizeof(leafNode));
            return p.data[index].second;
        }
        // Return a new iterator which points to the n-next elements
        iterator operator++(int) {
            iterator ret=*this;
            if(*this==bt->end()){
                ret.bt=NULL;ret.index=0;ret.offset=0;
                return ret;
            }
            leafNode lp;
            bt->readfile(&lp,offset,1,sizeof(leafNode));
            if(index==lp.inside-1){
                if(lp.next==0)++index;
                else{
                    offset=lp.next;
                    index=0;
                }
            }else{++index;}
            return ret;
        }
        iterator& operator++() {
            if(*this==bt->end()){
                bt=NULL;index=0;offset=0;
                return *this;
            }
            leafNode lp;
            bt->readfile(&lp,offset,1,sizeof(leafNode));
            if(index==lp.inside-1){
                if(lp.next==0)++index;
                else{
                    offset=lp.next;
                    index=0;
                }
            }else{++index;}
            return *this;
        }
        iterator operator--(int) {
            iterator ret=*this;
            if(*this==bt->begin()){
                ret.bt=NULL;ret.index=0;ret.offset=0;
                return ret;
            }
            leafNode lp,lq;
            bt->readfile(&lp,offset,1,sizeof(leafNode));
            if(index==0){
                    offset=lp.prev;
                    bt->readfile(&lq,lp.prev,1, sizeof(leafNode));
                    index=lq.inside-1;
            }else{--index;}
            return ret;
        }
        iterator& operator--() {
            if(*this==bt->begin()){
                bt=NULL;index=0;offset=0;
                return *this;
            }
            leafNode lp,lq;
            bt->readfile(&lp,offset,1,sizeof(leafNode));
            if(index==0){
                offset=lp.prev;
                bt->readfile(&lq,lp.prev,1, sizeof(leafNode));
                index=lq.inside-1;
            }else{--index;}
            return *this;
        }
        // Overloaded of operator '==' and '!='
        // Check whether the iterators are same
        bool operator==(const iterator& rhs) const {
            return (offset==rhs.offset&&bt==rhs.bt&&index==rhs.index);
        }
        bool operator==(const const_iterator& rhs) const {
            return (offset==rhs.offset&&bt==rhs.bt&&index==rhs.index);
        }
        bool operator!=(const iterator& rhs) const {
            return (offset!=rhs.offset||bt!=rhs.bt||index!=rhs.index);
        }
        bool operator!=(const const_iterator& rhs) const {
            return (offset!=rhs.offset||bt!=rhs.bt||index!=rhs.index);
        }
        value_type* operator->() const noexcept {
            /**
             * for the support of it->first.
             * See
             * <http://kelvinh.github.io/blog/2013/11/20/overloading-of-member-access-operator-dash-greater-than-symbol-in-cpp/>
             * for help.
             */
        }
    };
    class const_iterator {
        // it should has similar member method as iterator.
        //  and it should be able to construct from an iterator.
        friend class BTree;
    private:
        offset_t offset;//the offset of leafnode
        int index;
        BTree *bt;
    public:
        const_iterator(BTree *t=NULL,offset_t off=0,int in=0) {
            bt=t;index=in;offset=off;
        }

        const_iterator(const const_iterator& other) {
            bt=other.bt;
            offset=other.offset;
            index=other.index;
        }
        const_iterator(const iterator& other) {
            bt=other.bt;
            offset=other.offset;
            index=other.index;
        }
        Value getvalue(){
            leafNode p;
            bt->readfile(&p,offset,1, sizeof(leafNode));
            return p.data[index].second;
        }
        const_iterator operator++(int) {
            const_iterator ret=*this;
            if(*this==bt->end()){
                ret.bt=NULL;ret.index=0;ret.offset=0;
                return ret;
            }
            leafNode lp;
            bt->readfile(&lp,offset,1,sizeof(leafNode));
            if(index==lp.inside-1){
                if(lp.next==0)++index;
                else{
                    offset=lp.next;
                    index=0;
                }
            }else{++index;}
            return ret;
        }
        const_iterator& operator++() {
            if(*this==bt->end()){
                bt=NULL;index=0;offset=0;
                return *this;
            }
            leafNode lp;
            bt->readfile(&lp,offset,1,sizeof(leafNode));
            if(index==lp.inside-1){
                if(lp.next==0)++index;
                else{
                    offset=lp.next;
                    index=0;
                }
            }else{++index;}
            return *this;
        }
        const_iterator operator--(int) {
            const_iterator ret=*this;
            if(*this==bt->begin()){
                ret.bt=NULL;ret.index=0;ret.offset=0;
                return ret;
            }
            leafNode lp,lq;
            bt->readfile(&lp,offset,1,sizeof(leafNode));
            if(index==0){
                offset=lp.prev;
                bt->readfile(&lq,lp.prev,1, sizeof(leafNode));
                index=lq.inside-1;
            }else{--index;}
            return ret;
        }
        const_iterator& operator--() {
            if(*this==bt->begin()){
                bt=NULL;index=0;offset=0;
                return *this;
            }
            leafNode lp,lq;
            bt->readfile(&lp,offset,1,sizeof(leafNode));
            if(index==0){
                offset=lp.prev;
                bt->readfile(&lq,lp.prev,1, sizeof(leafNode));
                index=lq.inside-1;
            }else{--index;}
            return *this;
        }
        bool operator==(const iterator& rhs) const {
            return (offset==rhs.offset&&bt==rhs.bt&&index==rhs.index);
        }
        bool operator==(const const_iterator& rhs) const {
            return (offset==rhs.offset&&bt==rhs.bt&&index==rhs.index);
        }
        bool operator!=(const iterator& rhs) const {
            return (offset!=rhs.offset||bt!=rhs.bt||index!=rhs.index);
        }
        bool operator!=(const const_iterator& rhs) const {
            return (offset!=rhs.offset||bt!=rhs.bt||index!=rhs.index);
        }
        // And other methods in iterator, please fill by yourself.
    };


};
}  // namespace sjtu
