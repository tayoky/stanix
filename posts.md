---
title: Posts
permalink: /posts
---
Various posts about the developpement of Stanix.

<ul>
    {% for post in site.posts %}
        <li>
            <span class="post-date">{{ post.date | date : "%b %-d, %Y"}}</span>
            <a href="{{ post.url }}">{{ post.title}}</a>
        </li>
    {% endfor %}
</ul>
