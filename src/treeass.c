/*
  treeass.c : find the associations of the trees

  Time-stamp: <2001-05-13 14:11:31 shimo>

  shimo@ism.ac.jp 
  Hidetoshi Shimodaira

  typical usage:
  # foo.tpl -> foo.ass
  treeass foo
  # foo.tpl -> aho.ass
  treeass foo aho
*/

#include <stdio.h>
#include "misc.h"
#include "tree.h"

static const char rcsid[] = "$Id: treeass.c,v 1.1 2001/05/05 09:06:48 shimo Exp shimo $";

void putdot() {putchar('.'); fflush(STDOUT);}
void byebye() {error("error in command line");}

char *fname_tpl = NULL;
char *fname_ass = NULL;
char *fname_vt = NULL;
char *fext_tpl = ".tpl";
char *fext_ass = ".ass";
char *fext_vt = ".vt";

int ntree,nleaf,nsplit; /* numbers of trees, leaves, and splits */
Tnode **treevec; /* list of trees */
ivec **treesplit; /* split decompositions for the trees */
Wnode wlist; /* word list for labels */
Snode slist; /* split list */
Snode **splitvec; /* pointers to slist */

ivec *splitrev; /* indicate the equialent revserse split */
ivec *splitcom; /* splits common to all the trees */
ivec *splitbase; /* base splits */

ivec **splittree; /* the table from split to tree */

int sw_nleaf=0;
int sw_prtlabel=0;

int main(int argc, char** argv)
{
  /* working variables */
  int i,j,k,len;
  FILE *fp,*fpl,*fpr;
  Snode *sp;
  Wnode *wp;
  ivec *iv;
  int *ibuf,*orderv=NULL;
  Tnode **treevec0;
  int ntree0;
  char *cbuf;

  printf("# %s",rcsid);

  /* args */
  for(i=j=1;i<argc;i++) {
    if(argv[i][0] != '-') {
      switch(j) {
      case 1: fname_tpl=fname_ass=argv[i]; break;
      case 2: fname_ass=argv[i]; break;
      default: byebye();
      }
      j++;
    } else if(streq(argv[i],"-l")) {
      sw_nleaf=1;
    } else if(streq(argv[i],"-p")) {
      sw_prtlabel=1;
    } else if(streq(argv[i],"-d")) {
      if(i+1>=argc ||
	 sscanf(argv[i+1],"%d",&debugmode) != 1)
	byebye();
      i+=1;
    } else if(streq(argv[i],"-v")) {
      if(i+1>=argc) byebye();
      fname_vt=argv[i+1];
      i+=1;
    } else byebye();
  }

  /* open log-file */
  fpl=STDOUT;

  /* read tree topologies */
  if(fname_tpl){
    fp=openfp(fname_tpl,fext_tpl,"r",&cbuf);
    fprintf(fpl,"\n# reading %s",cbuf);
  } else {
    fprintf(fpl,"\n# reading from stdin"); fp=STDIN;
  }
  /* tpl file starts with the number of trees */
  if(sw_nleaf) nleaf=fread_i(fp);
  ntree0=fread_i(fp);
  /* init vectors and lists */
  treevec0=NEW_A(ntree0,Tnode*); wlist.next=NULL; 
  /* read trees */
  for(i=0;i<ntree0;i++) {
    treevec0[i]=fread_rtree(fp,&wlist); /* read */
  }
  FREE(cbuf); fclose(fp);
  fprintf(fpl,"\n# %d trees read",ntree0);

  /* selection */
  if(fname_vt) {
    fp=openfp(fname_vt,fext_vt,"r",&cbuf);
    printf("\n# reading %s",cbuf);
    ntree=0; orderv=fread_ivec(fp,&ntree);
    FREE(cbuf); fclose(fp);
    treevec=NEW_A(ntree,Tnode*);
    for(i=0;i<ntree;i++)
      treevec[i]=treevec0[orderv[i]];
    printf("\n# M: %d -> %d",ntree0,ntree);
  } else {
    treevec=treevec0; ntree=ntree0;
  }

  /* split decomposition */
  treesplit=NEW_A(ntree,ivec*); slist.next=NULL;
  for(i=0;i<ntree;i++) {
    sp=treetosplits(treevec[i]); /* get the split decomposition */
    treesplit[i]=splitstoids(sp,&slist); /* convert it to ivec */
  }

  /* count leaves */
  for(i=0,wp=wlist.next; wp!=NULL; i++,wp=wp->next);
  if(sw_nleaf)
    if(nleaf!=i) warning("no. of leaf=%d is different from %d\n",
			 i,nleaf);
  nleaf=i;

  fprintf(fpl,"\n# %d trees, %d leaves",ntree,nleaf);

  /* find the equivalent reverse split for unrooted trees */
  splitrev=unrootsplits(&slist);
  nsplit=splitrev->len;
  splitvec=NEW_A(nsplit,Snode*); /* this is easier to access than slist */
  for(i=0,sp=slist.next; i<nsplit; i++,sp=sp->next)
    splitvec[i]=sp;
  fprintf(fpl,"\n# edges total: %d",nsplit);
  for(i=j=0;i<nsplit;i++) if(splitrev->ve[i]!=i) j++;
  fprintf(fpl,"\n# reverse edges: %d",j);

  /* split decompositions are converted to unrooted version */
  for(i=0;i<ntree;i++) {  
    for(j=0;j<treesplit[i]->len;j++)
      treesplit[i]->ve[j]=splitrev->ve[treesplit[i]->ve[j]];
    isort(treesplit[i]->ve,NULL,treesplit[i]->len); /* looks neat */
  }

  /* find common splits */
  splitcom=isec_ivec(treesplit,ntree);

  /* subtract */
  for(i=0;i<ntree;i++) {
    iv=subt_ivec(treesplit[i],splitcom);
    FREE(treesplit[i]->ve); FREE(treesplit[i]);
    treesplit[i]=iv;
  }

  /* union */
  splitbase=unin_ivec(treesplit,ntree);

  /* renumbering of the base-splits */
  for(i=0;i<ntree;i++)
    for(j=0;j<treesplit[i]->len;j++)
      treesplit[i]->ve[j]=bsearch_ivec(splitbase,treesplit[i]->ve[j])-1;

  /* make reverse table from base-split to tree */  
  splittree=NEW_A(splitbase->len,ivec*);
  ibuf=NEW_A(ntree,int); 
  for(i=0;i<splitbase->len;i++){
    len=0;
    for(j=0;j<ntree;j++) {
      for(k=0;k<treesplit[j]->len;k++)
	if(treesplit[j]->ve[k]==i) break;
      if(k<treesplit[j]->len) {
	ibuf[len++]=j;
      }
    }
    splittree[i]=NEW(ivec);
    splittree[i]->len=len;
    if(len>0) {
      splittree[i]->ve=NEW_A(len,int);
      for(j=0;j<len;j++) splittree[i]->ve[j]=ibuf[j];
    } else splittree[i]->ve=NULL;
  }
  FREE(ibuf);


  /****************************************/

  fprintf(fpl,"\n# %d base-edges, %d common-edges, %d root-edge",
	  splitbase->len,splitcom->len-1,1);

  /* print trees */
  fprintf(fpl,"\n\n# trees: %d",ntree);
  fprintf(fpl,"\n%d\n",ntree);
  for(i=0;i<ntree;i++) {
    fwrite_rtree(fpl,treevec[i],sw_prtlabel?&wlist:NULL);
    fprintf(fpl," %d",i+1); 
    if(orderv) fprintf(fpl," <- %d",orderv[i]+1); 
    fprintf(fpl,"\n");
  }

  /* print leaves */
  fprintf(fpl,"\n# leaves: %d",nleaf);
  fprintf(fpl,"\n%d\n",nleaf);
  for(i=0,wp=wlist.next; i<nleaf; i++,wp=wp->next)
    fprintf(fpl,"%3d %s\n",i+1,wp->label);

  /* print the splits */
  fprintf(fpl,"\n# base edges: %d\n%d %d",
	  splitbase->len,splitbase->len,nleaf);
  fprintf(fpl,"\n    ");
  for(i=1;i<=nleaf;i++) fprintf(fpl,"%d",i%10);
  for(i=0;i<splitbase->len;i++) {
    sp=splitvec[splitbase->ve[i]];
    fprintf(fpl,"\n%3d ",i+1);
    for(j=1;j<sp->len;j++) 
      if(sp->set[j]==0) fprintf(fpl,"-");
      else if(sp->set[j]==1) fprintf(fpl,"+");
      else fprintf(fpl,"%d",sp->set[j]);
    for(;j<=nleaf;j++) fprintf(fpl,"-");
  }
  
  /* print the common splits */
  fprintf(fpl,"\n\n# common edges: %d\n%d %d",
	  splitcom->len-1,splitcom->len-1,nleaf);
  fprintf(fpl,"\n    ");
  for(i=1;i<=nleaf;i++) fprintf(fpl,"%d",i%10);
  for(i=1;i<splitcom->len;i++) { /* discard the first split, i.e. root */
    sp=splitvec[splitcom->ve[i]];
    fprintf(fpl,"\n%3d ",i+splitbase->len);
    for(j=1;j<sp->len;j++) 
      if(sp->set[j]==0) fprintf(fpl,"-");
      else if(sp->set[j]==1) fprintf(fpl,"+");
      else fprintf(fpl,"%d",sp->set[j]);
    for(;j<=nleaf;j++) fprintf(fpl,"-");
  }

  /* print tree->split association */
  fprintf(fpl,"\n\n# tree->edge\n%d",ntree);
  for(i=0;i<ntree;i++) {
    fprintf(fpl,"\n%3d %3d  ",i+1,treesplit[i]->len);
    for(j=0;j<treesplit[i]->len;j++)
      fprintf(fpl,"%d ",treesplit[i]->ve[j]+1);
  }

  /* print split->tree association */
  fprintf(fpl,"\n\n# edge->tree\n%d",splitbase->len);
  for(i=0;i<splitbase->len;i++) {
    fprintf(fpl,"\n%3d %3d ",i+1,splittree[i]->len);
    for(j=0;j<splittree[i]->len;j++) fprintf(fpl," %d",splittree[i]->ve[j]+1);
  }

  fprintf(fpl,"\n");

  /* OUTPUT ASSOCIATION */
  if(fname_ass) {
    fname_ass = mstrcat(fname_ass,fext_ass);
    if((fpr=fopen(fname_ass,"w"))==NULL) error("cant open %s",fname_ass);
    fprintf(fpl,"\n# writing %s",fname_ass);
  } else {
    fpr=STDOUT;
    fprintf(fpl,"\n# writing to stdout\n");
  }
  /* print split->tree association */
  fprintf(fpr,"\n# EDGE->TREE\n%d\n",splitbase->len);
  for(i=0;i<splitbase->len;i++) {
    fwrite_ivec(fpr,splittree[i]->ve,splittree[i]->len);
  }
  /* print tree->split association */
  fprintf(fpr,"\n# TREE->EDGE\n%d\n",ntree);
  for(i=0;i<ntree;i++) {
    fwrite_ivec(fpr,treesplit[i]->ve,treesplit[i]->len);
  }
  if(fname_ass) fclose(fpr);

  printf("\n# exit normally\n");
  exit(0);
}

