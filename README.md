# B+ Tree
---------------------------
## 简介
  这是数据结构（CS158）的课程大作业——写一棵B+树！但是快到考试周啦，只写完了查询和插入操作，没有写删除。  
### B+树的定义
M阶B+树被定义为有以下性质的M叉树：  
1.根或者是叶子，或者有2到M个孩子  
2.除根以外的所有节点都有不少于[M/2]且不多于M个孩子【节点至少达到半满，防止退化成简单的二叉树】  
3.有k个孩子的节点保存k-1个键引导查找，键i代表子树i+1中键的最小值  
4.叶节点孩子指针指向文件中存储记录的数据块地址  
5.每个数据块至少有[L/2]个记录，至多L个记录  
6.所有叶节点按序连成一个单链表  

B+树储存两个指针，一个指向树根（提供索引查找），一个指向关键词最小的叶节点（提供顺序访问）。  
为了提高磁盘访问的效率，一次磁盘读写正好读写一个完整的节点。其中，M和L是根据数据块容量、记录规模、关键词长度等来确定。  
## 实现方式
### B+树的储存
B+树本身的信息：  
```C++
struct treemeta{
      offset_t head;//first leaf
      offset_t tail;//tail of leaf
      offset_t root;//root
      size_t size;//size of bptree
      offset_t efo;//end of file
      treemeta():head(0),tail(0),root(0),size(0),efo(0){}
  };
 ```
 叶子节点：  
 ```C++
 struct leafNode{
      offset_t offset;
      offset_t parent;
      offset_t prev,next;
      int inside;//numbers of pair in leaf
      value_type data[L+1];//预留一个
      leafNode():offset(0),parent(0),prev(0),next(0),inside(0){}
  };
 ```
 非叶节点：
 ```C++
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
 ```
### 文件读写
高频次的外存访问（文件读写）成本非常高昂，因此对各个节点作严格4KB读写（通过计算M、L使节点大小接近4KB，提高文件利用效率）。读写由若干函数完成，主要是readfile()和writefile().
```C++
  void readfile(void *dest,offset_t off,int num,int size){
      fseek(fp,off,SEEK_SET);
      fread(dest,size,num,fp);
  }
  void writefile(void *dest,offset_t off,int num,int size){
      fseek(fp,off,SEEK_SET);
      fwrite(dest,size,num,fp);
  }
```
### B+树节点插入
#### 普通插入（节点未满）
从根节点开始查找插入的位置，将其插入对应的数据块。
#### 分裂（节点已满）
数据块满的情况下，需要将其分裂成两个数据块，并更新它们的父亲节点。若父亲节点的孩子未满，则直接插入新的孩子节点即可；若父亲节点孩子已满，则需要再分裂父亲。如此继续向上分裂，直到不再需要分裂或到达了根节点。  


  

