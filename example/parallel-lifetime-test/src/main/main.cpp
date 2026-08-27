#include	<cstdio>
#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwMain.h"

extern volatile int plt_ran;
extern int          plt_want;

int
main(int argc, char** argv)
{
	::setvbuf(stdout, NULL, _IONBF, 0);
	thNEW(tsApplication,(thNULL,[](sPtr<tsApplication> app){
		thNEW(hwMain,(app));
	}));
	/* 判定を main に置くのは、失敗した場合に「chain が途切れたまま group は正常完了
	 * を報告する」ため。状態機械の中では成功と区別がつかない。 */
	return plt_ran == plt_want ? 0 : 1;
}
