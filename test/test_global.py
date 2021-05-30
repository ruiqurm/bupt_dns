import webbrowser
import asyncio

async def op(url):
    await asyncio.sleep(0.01) 
    webbrowser.open(url)

if __name__ == "__main__":
    loop = asyncio.get_event_loop()
    url_list = ['http://www.4399.com/','http://www.4399.com/','http://www.4399.com/','http://www.4399.com/',]*10
    tasks = []
    for url in url_list:
        c = op(url)
        tasks.append(c) 
    loop.run_until_complete(asyncio.wait(tasks))
    loop.close()